import asyncio
from collections.abc import Awaitable, Callable

from CommonClient import logger

# The door the mod knocks on: this PC only, port 2727.
HOST = "127.0.0.1"
PORT = 2727


class ModLink:
    """Local server that GTA III's mod connects to; every message is one text line (see docs/protocol.md)."""

    def __init__(self, on_check: Callable[[str], Awaitable[None]], on_mod_connected: Callable[[], None]) -> None:
        # Called with the mission's text key each time the mod sends "check <key>"; ModLink does not know what it does.
        self.on_check = on_check
        # Called each time the mod connects, right after the room state, so the context sends it every item received so far.
        self.on_mod_connected = on_mod_connected
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

    # Runs once per mod connection: tells the mod the room state, then handles its lines until the game closes.
    async def handle_mod(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        if self.writer:
            self.writer.close()
        self.writer = writer
        logger.info("GTA III connected")
        self.send_status()
        self.on_mod_connected()
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
        else:
            logger.warning(f"Unknown message from GTA III: {line}")

    # Remembers whether we are connected to the room and tells the mod.
    def set_room_connected(self, connected: bool) -> None:
        self.room_connected = connected
        self.send_status()

    # Sends the current room state to the mod.
    def send_status(self) -> None:
        self.send("status connected" if self.room_connected else "status disconnected")

    # Sends one received item to the mod: its position in our slot's list, what it does in the game ("unlock 24", "money 10000", "none 0") and its name.
    def send_item(self, index: int, effect: str, value: int, name: str) -> None:
        self.send(f"item {index} {effect} {value} {name}")

    # Sends one line to the mod, if it is connected.
    def send(self, message: str) -> None:
        if self.writer:
            self.writer.write((message + "\n").encode("utf-8"))
