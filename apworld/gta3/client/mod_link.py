import asyncio
from collections.abc import Awaitable, Callable

from CommonClient import logger

# The door the mod knocks on: this PC only, port 2727.
HOST = "127.0.0.1"
PORT = 2727
# The player's state the client keeps for our slot between sessions, in its save file, as numbers; the weapons held come with it, each with its ammo.
PLAYER_NUMBERS = ("money", "health", "armour", "in_hand")
# The weapons a player can hold, by their number in the game (eWeaponType): 1, the bat, to 12, the detonator.
WEAPON_NUMBERS = range(1, 13)


def clean_player(raw: object) -> dict:
    """The player's state from a save file or a line, keeping only what is valid: whole numbers of 0 or more, and weapons as [number, ammo] of a weapon the player can hold."""
    def number(value: object) -> bool:
        return isinstance(value, int) and not isinstance(value, bool) and value >= 0

    if not isinstance(raw, dict):
        return {}
    player = {name: raw[name] for name in PLAYER_NUMBERS if number(raw.get(name))}
    weapons = raw.get("weapons", [])
    if isinstance(weapons, list):
        player["weapons"] = [[weapon[0], weapon[1]] for weapon in weapons
                             if isinstance(weapon, list) and len(weapon) == 2 and number(weapon[0]) and weapon[0] in WEAPON_NUMBERS and number(weapon[1])]
    return player


def player_words(player: dict) -> list[str]:
    """The player's state as the protocol writes it: "money=12000", ..., then "weapon=3:250" for each weapon."""
    return [f"{name}={player[name]}" for name in PLAYER_NUMBERS if name in player] + [f"weapon={weapon}:{ammo}" for weapon, ammo in player.get("weapons", [])]


def read_words(words: list[str]) -> dict | None:
    """The values of "key=value" words, as player_words and the mod write them, with "given_index" among them; None if a word is none of them."""
    values: dict = {"weapons": []}
    for word in words:
        key, _, value = word.partition("=")
        if key == "weapon":
            weapon, _, ammo = value.partition(":")
            if not (weapon.isdecimal() and ammo.isdecimal()):
                return None
            values["weapons"].append([int(weapon), int(ammo)])
        elif key in (*PLAYER_NUMBERS, "given_index") and value.isdecimal():
            values[key] = int(value)
        else:
            return None
    return values


class ModLink:
    """Local server that GTA III's mod connects to; every message is one text line (see docs/protocol.md)."""

    def __init__(self, on_check: Callable[[str], Awaitable[None]], on_death: Callable[[str], Awaitable[None]], on_progress: Callable[[int, dict], Awaitable[None]],
                 on_mod_connected: Callable[[], None]) -> None:
        # Called with the mission's text key each time the mod sends "check <key>"; ModLink does not know what it does.
        self.on_check = on_check
        # Called each time the mod connects, right before the room state, so the context sends it everything of our slot first.
        self.on_mod_connected = on_mod_connected
        # Called with "wasted" or "busted" each time the mod sends "death <how>".
        self.on_death = on_death
        # Called with the count of items applied and the player's state each time the mod sends "progress given_index=<count> money=<amount> ...", all read on the same frame of the game.
        self.on_progress = on_progress
        self.room_connected = False
        self.writer: asyncio.StreamWriter | None = None
        self.server: asyncio.Server | None = None

    # Opens the door on 127.0.0.1:2727; the mod can then connect at any time.
    async def start(self) -> None:
        try:
            self.server = await asyncio.start_server(self.handle_mod, HOST, PORT)
        except OSError:
            logger.error(f"Port {PORT} is already in use: is another GTA3 Client open?")
            return
        logger.info(f"Waiting for GTA III on {HOST}:{PORT}")

    # Closes the door and the current mod connection when the client exits.
    def stop(self) -> None:
        if self.writer:
            self.writer.close()
        if self.server:
            self.server.close()

    # Runs once per mod connection: tells the mod everything of our slot, then the room state, then handles its lines until the game closes.
    async def handle_mod(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        if self.writer:
            self.writer.close()
        self.writer = writer
        logger.info("GTA III connected")
        self.on_mod_connected()
        self.send_status()
        try:
            while line := await reader.readline():
                await self.handle_line(line.decode("utf-8", errors="replace").strip())
        except ConnectionError:
            pass
        finally:
            if self.writer is writer:
                self.writer = None
                logger.info("GTA III disconnected")
            writer.close()

    # Applies one line received from the mod; unknown lines are logged and ignored, like the mod does.
    async def handle_line(self, line: str) -> None:
        words = line.split(" ")
        if len(words) == 2 and words[0] == "check":
            await self.on_check(words[1])
        elif len(words) == 2 and words[0] == "death":
            await self.on_death(words[1])
        elif words[0] == "progress" and (values := read_words(words[1:])) is not None and "given_index" in values:
            await self.on_progress(values.pop("given_index"), clean_player(values))
        else:
            logger.warning(f"Unknown message from GTA III: {line}")

    # Remembers whether we are connected to the room and tells the mod.
    def set_room_connected(self, connected: bool) -> None:
        self.room_connected = connected
        self.send_status()

    # Sends the current room state to the mod.
    def send_status(self) -> None:
        self.send("status connected" if self.room_connected else "status disconnected")

    # Tells the mod another DeathLink player died, by their name: the mod kills the player.
    def send_kill(self, name: str) -> None:
        self.send(f"kill {name}")

    # Tells the mod how many items of our slot's list its earlier sessions applied ("given_index 5"), from the save file.
    def send_given_index(self, count: int) -> None:
        self.send(f"given_index {count}")

    # Sends the mod the player's state kept from our slot's earlier sessions ("player money=12000 health=100 ... weapon=3:250"; "player" alone for none): a game that starts takes it.
    def send_player(self, player: dict) -> None:
        self.send(" ".join(["player", *player_words(player)]))

    # Tells the mod the effects of the consumable items ("consumables money"): a game that starts skips those an earlier session applied.
    def send_consumables(self, effects: list[str]) -> None:
        self.send(" ".join(["consumables", *effects]))

    # Sends one of our slot's options to the mod, by its name in the YAML and its value as a number ("option need_money 0").
    def send_option(self, name: str, value: int) -> None:
        self.send(f"option {name} {value}")

    # Tells the mod which weapons are items in our slot, by their number in the game ("weapons 3 4 5"; "weapons" alone for none): a shop sells each one only once its item is received.
    def send_weapons(self, weapons: list[int]) -> None:
        self.send(" ".join(["weapons", *map(str, weapons)]))

    # Sends one received item to the mod: its position in our slot's list, what it does in the game ("unlock 24", "money 10000", "weapon 3", "none 0") and its name.
    def send_item(self, index: int, effect: str, value: int, name: str) -> None:
        self.send(f"item {index} {effect} {value} {name}")

    # Sends one line to the mod, if it is connected.
    def send(self, message: str) -> None:
        if self.writer:
            self.writer.write((message + "\n").encode("utf-8"))
