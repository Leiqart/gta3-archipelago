"""Grand Theft Auto III — Archipelago text client.

Launchable from the Archipelago Launcher ("GTA3 Client"): connects to a room
as the GTA3 slot and answers progression questions straight from the server
state (no game required). The in-game bridge keeps running independently;
this client is the analysis/requests companion.

Commands (type them in the client input):
  /progression   per-contact chain status: done / playable now / waiting on
  /playable      just the missions you can start right now
  /missing       unlock items not received yet, and where the chain stalls
"""
import asyncio

from CommonClient import (
    ClientCommandProcessor,
    CommonContext,
    get_base_parser,
    gui_enabled,
    server_loop,
)

from .data import data

GAME = "Grand Theft Auto III"

LOC_ID_BY_NAME = dict(data["location_ids"])
LOC_NAME_BY_ID = {v: k for k, v in LOC_ID_BY_NAME.items()}
ITEM_ID_BY_NAME = dict(data["item_ids"])
ITEM_NAME_BY_ID = {v: k for k, v in ITEM_ID_BY_NAME.items()}
UNLOCK_BY_LOCATION = dict(data.get("mission_unlock_by_location", {}))


def build_chains():
    """[(chain, [location ids in order])] following data['locations'] order."""
    chains = {}
    order = []
    for loc in data["locations"]:
        chain = loc.get("chain")
        if not chain:
            continue
        if chain not in chains:
            chains[chain] = []
            order.append(chain)
        chains[chain].append(loc["id"])
    return [(chain, chains[chain]) for chain in order]


class GTA3CommandProcessor(ClientCommandProcessor):
    def _chain_report(self):
        ctx = self.ctx
        checked = {LOC_NAME_BY_ID.get(i) for i in ctx.checked_locations}
        received = {ITEM_NAME_BY_ID.get(item.item) for item in ctx.items_received}
        report = []
        for chain, locs in build_chains():
            done = sum(1 for l in locs if l in checked)
            playable = None
            waiting_on = None
            for loc in locs:
                if loc in checked:
                    continue
                unlock = UNLOCK_BY_LOCATION.get(loc)
                if unlock is None or unlock in received:
                    playable = loc
                else:
                    waiting_on = unlock
                break  # chains are strictly ordered: only the first open slot matters
            report.append((chain, done, len(locs), playable, waiting_on))
        return report

    def _cmd_progression(self):
        """Per-contact chain status: done / playable now / waiting on item."""
        if not self.ctx.server or not self.ctx.slot:
            self.output("Not connected.")
            return
        for chain, done, total, playable, waiting in self._chain_report():
            if done == total:
                status = "TERMINEE"
            elif playable:
                status = f"JOUABLE -> {playable}"
            elif waiting:
                status = f"attend {waiting}"
            else:
                status = "?"
            self.output(f"{chain:<16} {done}/{total}  {status}")

    def _cmd_playable(self):
        """Missions you can start right now."""
        if not self.ctx.server or not self.ctx.slot:
            self.output("Not connected.")
            return
        any_playable = False
        for chain, _done, _total, playable, _waiting in self._chain_report():
            if playable:
                self.output(f"{playable}  ({chain})")
                any_playable = True
        if not any_playable:
            self.output("Aucune mission jouable: farme packages/RC pour des checks.")

    def _cmd_missing(self):
        """Unlock items not received yet (where each chain stalls)."""
        if not self.ctx.server or not self.ctx.slot:
            self.output("Not connected.")
            return
        received = {ITEM_NAME_BY_ID.get(item.item) for item in self.ctx.items_received}
        shown = False
        for chain, done, total, _playable, waiting in self._chain_report():
            if waiting and waiting not in received:
                self.output(f"{chain:<16} bloquee ({done}/{total}) sur {waiting}")
                shown = True
        if not shown:
            self.output("Aucune chaine bloquee par un item manquant.")


class GTA3Context(CommonContext):
    game = GAME
    items_handling = 0b111
    command_processor = GTA3CommandProcessor

    async def server_auth(self, password_requested: bool = False):
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()


def launch(*launch_args):
    async def main(args):
        ctx = GTA3Context(args.connect, args.password)
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        await ctx.exit_event.wait()
        await ctx.shutdown()

    parser = get_base_parser(description="GTA3 Archipelago text client.")
    args = parser.parse_args(launch_args)
    import colorama
    colorama.just_fix_windows_console()
    asyncio.run(main(args))
    colorama.deinit()
