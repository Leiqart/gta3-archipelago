import asyncio

import Utils
from CommonClient import CommonContext, get_base_parser, gui_enabled, handle_url_arg, logger, server_loop

from ..missions import ALL_MISSIONS, CONTACTS, MISSIONS, MONEY_AMOUNT, MONEY_ID, contact_missions
from .mod_link import ModLink

# The location of each text key the game registers when a mission is passed: the mod sends the key, the room wants the id.
LOCATION_OF_KEY = {mission.key: mission.location_id for mission in ALL_MISSIONS}
# What each unlock and progressive item unlocks: a mission's number in the game, which the mod's markers know.
MISSION_OF_UNLOCK = {mission.unlock_id: mission.number for mission in MISSIONS if mission.unlock_id}
CHAIN_OF_PROGRESSIVE = {contact.progressive_id: [mission.number for mission in contact_missions(contact.name)] for contact in CONTACTS}


class GTA3Context(CommonContext):
    """Joins the Archipelago room as a GTA III slot, relays the room state and our items to the mod, and the mod's checks to the room."""
    game = "Grand Theft Auto III"
    # Receive every item: from other games (0b001), from our own game (0b010) and the starting inventory (0b100).
    items_handling = 0b111

    def __init__(self, server_address: str | None, password: str | None) -> None:
        super().__init__(server_address, password)
        self.mod = ModLink(on_check=self.on_mod_check, on_mod_connected=self.send_items_to_mod)

    # Called by ModLink for each "check <key>" line: sends the location of that mission to the room, which ignores ids it already has.
    async def on_mod_check(self, key: str) -> None:
        location_id = LOCATION_OF_KEY.get(key)
        if location_id is None:
            logger.info(f"Mission {key} passed, it is not a location")
        elif not self.mod.room_connected:
            logger.info(f"Check {key} not sent: not connected to the room")
        elif not await self.check_locations([location_id]):
            logger.info(f"Check {key} not sent: already checked or unknown location")

    # What an item does in the game, as the mod applies it: ("unlock", mission number), ("money", amount), or ("none", 0).
    # A progressive item unlocks its contact's next mission: the nth copy received, counted up to this index, unlocks the nth mission.
    def item_effect(self, index: int) -> tuple[str, int]:
        item_id = self.items_received[index].item
        if item_id == MONEY_ID:
            return "money", MONEY_AMOUNT
        if item_id in MISSION_OF_UNLOCK:
            return "unlock", MISSION_OF_UNLOCK[item_id]
        chain = CHAIN_OF_PROGRESSIVE.get(item_id)
        if chain is None:
            return "none", 0
        copy = sum(1 for item in self.items_received[:index + 1] if item.item == item_id)
        return ("unlock", chain[copy - 1]) if copy <= len(chain) else ("none", 0)

    # Relays to the mod the items our slot has received, from this position in the list to its end, each with its effect and its name.
    def send_items_to_mod(self, start: int = 0) -> None:
        for index in range(start, len(self.items_received)):
            effect, value = self.item_effect(index)
            self.mod.send_item(index, effect, value, self.item_names.lookup_in_slot(self.items_received[index].item))

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

    # Reacts to the room's messages: "Connected" means our slot is ready, "ReceivedItems" brings items from this index on (0 resends them all).
    def on_package(self, cmd: str, args: dict) -> None:
        super().on_package(cmd, args)
        if cmd == "Connected":
            self.mod.set_room_connected(True)
        elif cmd == "ReceivedItems":
            self.send_items_to_mod(args["index"])

    # Runs whenever the link to the room is lost, whatever the reason.
    async def connection_closed(self) -> None:
        await super().connection_closed()
        self.mod.set_room_connected(False)


# Entry point of the "GTA3 Client" button: opens the client window and the door for the mod.
def launch(*launch_args: str) -> None:
    async def main(args) -> None:
        ctx = GTA3Context(args.connect, args.password)
        ctx.auth = args.name
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
        await ctx.mod.start()
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        await ctx.exit_event.wait()
        ctx.mod.stop()
        await ctx.shutdown()

    Utils.init_logging("GTA3Client", exception_logger="Client")
    parser = get_base_parser(description="GTA III Archipelago client.")
    parser.add_argument("--name", default=None, help="Slot name to connect as.")
    parser.add_argument("url", nargs="?", help="Connection url, like archipelago://name@host:port")
    args = handle_url_arg(parser.parse_args(launch_args), parser=parser)
    asyncio.run(main(args))
