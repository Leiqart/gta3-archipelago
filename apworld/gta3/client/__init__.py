import asyncio
import copy
import json
import os

import Utils
from CommonClient import CommonContext, get_base_parser, gui_enabled, handle_url_arg, logger, server_loop
from NetUtils import ClientStatus

from ..items import CONSUMABLE_EFFECTS, effects_of
from ..missions import ALL_MISSIONS, CLOSES, GOAL_MISSION, mission_named
from .mod_link import ModLink, clean_player, player_words

# The location of each text key the game registers when a mission is passed: the mod sends the key, the room wants the id.
LOCATION_OF_KEY = {mission.key: mission.location_id for mission in ALL_MISSIONS}
# The locations of the missions each closer's pass loses for good, by the closer's key: they are checked with it, as the game will never pass them.
LOST_WITH_KEY = {mission_named(closer).key: [mission_named(name).location_id for name in closed] for closer, closed in CLOSES.items()}
# The key the game registers when the goal mission is passed: its check also tells the room our slot has won.
GOAL_KEY = next(mission.key for mission in ALL_MISSIONS if mission.name == GOAL_MISSION)
# The folder, in Archipelago's own, of the files that keep what a slot's sessions leave for the next one.
SAVES_FOLDER = "gta3_saves"
# How often, in seconds, the saver compares what the game has with what the save file has, besides each "progress" line of the mod.
SAVE_EVERY = 30


class GTA3Context(CommonContext):
    """Joins the Archipelago room as a GTA III slot, relays the room state and our items to the mod, and the mod's checks to the room."""
    game = "Grand Theft Auto III"
    # Receive every item: from other games (0b001), from our own game (0b010) and the starting inventory (0b100).
    items_handling = 0b111

    def __init__(self, server_address: str | None, password: str | None) -> None:
        super().__init__(server_address, password)
        self.mod = ModLink(on_check=self.on_mod_check, on_death=self.on_mod_death, on_progress=self.on_mod_progress, on_mod_connected=self.send_slot_to_mod)
        # Our slot's options the mod plays by, from the room's slot data; empty until the room has connected us.
        self.slot_options: dict[str, int] = {}
        # What our slot keeps between sessions, {"given_index": <count>, "player": <state>}, as the game has it now, and as its save file has it: the saver writes the file when they differ.
        # Both None until the room has connected us: the mod hears nothing of the room before.
        self.kept: dict | None = None
        self.saved: dict | None = None
        # The save file of the room and slot the client is connected to; None before.
        self.save_file: str | None = None
        # The seed of the room, from its RoomInfo: the save file's name starts with it, so each room has its own.
        self.room_seed: str | None = None
        # The weapons that are items in our slot, by their number in the game, from the room's slot data; None until the room has connected us.
        self.weapon_items: list[int] | None = None
        # Whether being busted sends a death to the DeathLink players, as the slot data says.
        self.arrest_is_death = True

    # Called by ModLink for each "check <key>" line: sends the location of that mission to the room, which ignores ids it already has; a closer's also sends the missions it loses, the goal mission's also wins.
    async def on_mod_check(self, key: str) -> None:
        location_id = LOCATION_OF_KEY.get(key)
        if location_id is None:
            logger.info(f"Mission {key} passed, it is not a location")
            return
        if not self.mod.room_connected:
            logger.info(f"Check {key} not sent: not connected to the room")
            return
        if not await self.check_locations([location_id]):
            logger.info(f"Check {key} not sent: already checked or unknown location")
        if key in LOST_WITH_KEY:
            await self.check_lost_missions(key)
        if key == GOAL_KEY:
            await self.send_goal()

    # Checks the missions this closer's pass has lost for good; those already passed are already checked, and the room ignores them.
    async def check_lost_missions(self, key: str) -> None:
        sent = await self.check_locations(LOST_WITH_KEY[key])
        if sent:
            names = ", ".join(self.location_names.lookup_in_slot(location_id) for location_id in sorted(sent))
            logger.info(f"Mission {key} closed {len(sent)} missions of the story, checked too: {names}")

    # Called by ModLink for each "death wasted|busted" line: sends our death to the DeathLink players, if our slot is one, and a busted only when it counts.
    async def on_mod_death(self, how: str) -> None:
        if "DeathLink" not in self.tags:
            logger.info(f"The player was {how}: DeathLink is off")
            return
        if how == "busted" and not self.arrest_is_death:
            logger.info("The player was busted: arrest_is_death is off, no death sent")
            return
        await self.send_death(f"{self.player_names[self.slot]} was {how} in Liberty City")

    # Our slot's save file, in Archipelago's folder: one per room (its seed), team and slot, so that a new room starts from nothing.
    def save_path(self) -> str:
        return Utils.user_path(SAVES_FOLDER, f"{self.room_seed}_{self.team}_{self.slot}.json")

    # What our slot's save file keeps: no file (or none known) or an unreadable one keeps nothing, given_index 0 and an empty state; a value that is not valid is left out (see clean_player).
    def load_kept(self) -> dict:
        try:
            with open(self.save_file, encoding="utf-8") as file:
                saved = json.load(file)
        except (OSError, TypeError, ValueError):
            saved = {}
        if not isinstance(saved, dict):
            saved = {}
        given_index = saved.get("given_index")
        valid = isinstance(given_index, int) and not isinstance(given_index, bool) and given_index >= 0
        return {"given_index": given_index if valid else 0, "player": clean_player(saved.get("player"))}

    # The kept values in words, for the log.
    def describe_kept(self) -> str:
        return " ".join([f"given_index={self.kept['given_index']}", *player_words(self.kept["player"])])

    # Compares what the game has with what the save file has: nothing to do when they are the same; otherwise writes the file whole, one change or several at once,
    # into a new file first, then in place of the old one, so that a client closed while writing leaves the previous file.
    def save_if_changed(self) -> None:
        if self.kept is None or self.save_file is None or self.kept == self.saved:
            return
        os.makedirs(os.path.dirname(self.save_file), exist_ok=True)
        with open(self.save_file + ".tmp", "w", encoding="utf-8") as file:
            json.dump(self.kept, file)
        os.replace(self.save_file + ".tmp", self.save_file)
        self.saved = copy.deepcopy(self.kept)
        logger.info("Saved for the next session: " + self.describe_kept())

    # Runs in the background while the client is open: every SAVE_EVERY seconds, writes the save file if what the game has changed.
    async def saver(self) -> None:
        while not self.exit_event.is_set():
            try:
                await asyncio.wait_for(self.exit_event.wait(), SAVE_EVERY)
            except asyncio.TimeoutError:
                pass
            self.save_if_changed()

    # Called by ModLink for each "progress given_index=<count> money=<amount> ..." line: what the game has now, all read on the same frame; the count of items applied only grows.
    async def on_mod_progress(self, applied: int, player: dict) -> None:
        if self.kept is None:
            logger.info(f"Progress given_index={applied} not kept: not connected to the room")
            return
        self.kept = {"given_index": max(applied, self.kept["given_index"]), "player": player}
        self.save_if_changed()

    # A DeathLink player died elsewhere: the client logs it as usual, and the mod kills the player.
    def on_deathlink(self, data: dict) -> None:
        super().on_deathlink(data)
        self.mod.send_kill(data.get("source", "Someone"))

    # Tells the room our slot has reached its goal: it announces it and, depending on the room's settings, releases our remaining items; sending it again changes nothing.
    async def send_goal(self) -> None:
        await self.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}])
        self.finished_game = True
        logger.info(f"Goal reached: {GOAL_MISSION} is passed")

    # Relays to the mod everything of our slot it plays by, once the room has connected us: the consumable effects and the kept values first, as a game takes them before its items,
    # then the options, then every item received.
    def send_slot_to_mod(self) -> None:
        if self.kept is None:
            return
        self.mod.send_consumables(CONSUMABLE_EFFECTS)
        self.mod.send_given_index(self.kept["given_index"])
        self.mod.send_player(self.kept["player"])
        self.send_options_to_mod()
        self.send_items_to_mod()

    # Relays to the mod what the room's slot data says of how our slot plays, once the room has said it: each option, then the weapons that are items.
    def send_options_to_mod(self) -> None:
        for name, value in self.slot_options.items():
            self.mod.send_option(name, value)
        if self.weapon_items is not None:
            self.mod.send_weapons(self.weapon_items)

    # Relays to the mod the items our slot has received, from this position in the list to its end, each with what items.py says it does in the game and its name:
    # the whole list when the mod connects, the new ones as the room gives them.
    def send_items_to_mod(self, start: int = 0) -> None:
        received = [item.item for item in self.items_received]
        for index, effect, value in effects_of(received, start):
            self.mod.send_item(index, effect, value, self.item_names.lookup_in_slot(received[index]))

    # Gives the client window its own title.
    def make_gui(self):
        ui = super().make_gui()
        ui.base_title = "Archipelago GTA III Client"
        return ui

    # Asks for the slot name if it is missing, then connects to that slot.
    async def server_auth(self, password_requested: bool = False) -> None:
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    # Reacts to the room's messages: "RoomInfo" names the room, before anything else; "Connected" means our slot is ready, "ReceivedItems" brings items from this index on (0 resends them all).
    def on_package(self, cmd: str, args: dict) -> None:
        super().on_package(cmd, args)
        if cmd == "RoomInfo":
            self.room_seed = args.get("seed_name")
        elif cmd == "Connected":
            # A slot generated before an option existed has none in its slot data: it plays the game's way (money needed, no DeathLink, every weapon sold as the story says).
            slot_data = args["slot_data"]
            self.slot_options = {"need_money": int(slot_data.get("need_money", True))}
            self.weapon_items = [int(weapon) for weapon in slot_data.get("weapon_items", [])]
            self.arrest_is_death = bool(slot_data.get("arrest_is_death", True))
            Utils.async_start(self.update_death_link(bool(slot_data.get("death_link", False))), name="update death link")
            # The save file of this room and slot, read before the mod hears of the room: a game must not start without it. The mod then gets our whole slot, and the room state last.
            if not self.room_seed:
                logger.error("The room did not say its seed: nothing is kept between sessions")
            self.save_file = self.save_path() if self.room_seed else None
            self.kept = self.load_kept()
            self.saved = copy.deepcopy(self.kept)
            logger.info("Kept from this slot's earlier sessions: " + self.describe_kept())
            self.send_slot_to_mod()
            self.mod.set_room_connected(True)
        elif cmd == "ReceivedItems" and self.kept is not None:
            self.send_items_to_mod(args["index"])

    # Runs whenever the link to the room is lost, whatever the reason: what the game had is saved first, and the save file is read again when the room connects us again, maybe to another slot.
    async def connection_closed(self) -> None:
        self.save_if_changed()
        await super().connection_closed()
        self.kept = self.saved = None
        self.mod.set_room_connected(False)


# Entry point of the "GTA3 Client" button: opens the client window and the door for the mod.
def launch(*launch_args: str) -> None:
    async def main(args) -> None:
        ctx = GTA3Context(args.connect, args.password)
        ctx.auth = args.name
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
        await ctx.mod.start()
        saver = asyncio.create_task(ctx.saver(), name="saver")
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        await ctx.exit_event.wait()
        await saver
        ctx.mod.stop()
        # What the game had last, in case the saver's last turn came before it.
        ctx.save_if_changed()
        await ctx.shutdown()

    Utils.init_logging("GTA3Client", exception_logger="Client")
    parser = get_base_parser(description="GTA III Archipelago client.")
    parser.add_argument("--name", default=None, help="Slot name to connect as.")
    parser.add_argument("url", nargs="?", help="Connection url, like archipelago://name@host:port")
    args = handle_url_arg(parser.parse_args(launch_args), parser=parser)
    asyncio.run(main(args))
