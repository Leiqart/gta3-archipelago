import asyncio

import Utils
from CommonClient import CommonContext, get_base_parser, gui_enabled, handle_url_arg, logger, server_loop

from .mod_link import ModLink


class GTA3Context(CommonContext):
    """Joins the Archipelago room as a GTA III slot, relays the room state to the mod and the mod's checks to the room."""
    game = "Grand Theft Auto III"
    # Receive every item: from other games (0b001), from our own game (0b010) and the starting inventory (0b100).
    items_handling = 0b111

    def __init__(self, server_address: str | None, password: str | None) -> None:
        super().__init__(server_address, password)
        self.mod = ModLink(on_check=self.on_mod_check)

    # Called by ModLink for each "check <id>" line: sends that location to the room, which ignores ids it already has.
    async def on_mod_check(self, location_id: int) -> None:
        if not self.mod.room_connected:
            logger.info(f"Check {location_id} not sent: not connected to the room")
        elif not await self.check_locations([location_id]):
            logger.info(f"Check {location_id} not sent: already checked or unknown location")

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

    # Reacts to the room's messages; "Connected" means our slot is ready.
    def on_package(self, cmd: str, args: dict) -> None:
        super().on_package(cmd, args)
        if cmd == "Connected":
            self.mod.set_room_connected(True)

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
