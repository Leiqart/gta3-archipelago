"""GTA3 Client: joins the Archipelago room and relays it to the in-game ASI on 127.0.0.1:2727."""
import asyncio

from CommonClient import (
    ClientCommandProcessor,
    CommonContext,
    get_base_parser,
    gui_enabled,
    logger,
    server_loop,
)
from NetUtils import ClientStatus

from .data import data
from . import (
    build_mission_access_requirements,
    build_previous_location_by_chain,
    build_story_links_by_target,
)
from .protocol import (
    GAME_ID,
    LOCAL_HOST,
    LOCAL_PORT,
    MAX_MESSAGE_BYTES,
    PROTOCOL_VERSION,
    ProtocolError,
    decode_game_message,
    encode_message,
)

GAME = "Grand Theft Auto III"

LOC_ID_BY_NAME = dict(data["location_ids"])
LOC_NAME_BY_ID = {v: k for k, v in LOC_ID_BY_NAME.items()}
ITEM_ID_BY_NAME = dict(data["item_ids"])
ITEM_NAME_BY_ID = {v: k for k, v in ITEM_ID_BY_NAME.items()}
UNLOCK_BY_LOCATION = dict(data.get("mission_unlock_by_location", {}))
LOCATION_BY_NAME = {loc["id"]: loc for loc in data["locations"]}
PREVIOUS_BY_LOCATION = build_previous_location_by_chain()
STORY_LINKS_BY_TARGET = build_story_links_by_target()


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
    @staticmethod
    def _mission_wait_reason(location_id, checked, received):
        loc = LOCATION_BY_NAME[location_id]
        region = loc["region"]
        if region != "Portland":
            region_item = data["region_item"][region]
            region_story = data["region_story_requirements"][region]
            if region_item not in received and region_story not in checked:
                return f"{region_item} OU FINIR {region_story}"

        requirements = build_mission_access_requirements(
            location_id, PREVIOUS_BY_LOCATION, STORY_LINKS_BY_TARGET)
        unlock_item = requirements["unlock_item"]
        openers = requirements["opener_triggers"]
        if openers:
            item_route = bool(unlock_item) and unlock_item in received
            story_route = any(trigger in checked for trigger in openers)
            if not item_route and not story_route:
                story_text = " / ".join(f"FINIR {trigger}" for trigger in openers)
                return (f"{unlock_item} OU {story_text}"
                        if unlock_item else story_text)
        elif unlock_item and unlock_item not in received:
            return unlock_item

        missing_parents = [
            parent for parent in requirements["prerequisites"]
            if parent not in checked
        ]
        if missing_parents:
            return "FINIR " + " + ".join(missing_parents)
        return None

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
                waiting_on = self._mission_wait_reason(loc, checked, received)
                if waiting_on is None:
                    playable = loc
                break  # only the first unfinished slot of a chain can advance
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
        shown = False
        for chain, done, total, _playable, waiting in self._chain_report():
            if waiting:
                self.output(f"{chain:<16} bloquee ({done}/{total}) sur {waiting}")
                shown = True
        if not shown:
            self.output("Aucune chaine bloquee par un item manquant.")


class GTA3Context(CommonContext):
    game = GAME
    items_handling = 0b111
    command_processor = GTA3CommandProcessor

    def __init__(self, server_address=None, password=None):
        super().__init__(server_address, password)
        self.game_writer = None
        self.game_items_sent = 0
        self._background_tasks = set()
        self._room_identity = None

    def start_task(self, coroutine):
        task = asyncio.create_task(coroutine)
        self._background_tasks.add(task)
        task.add_done_callback(self._background_tasks.discard)
        return task

    async def resend_progress(self):
        if not self.server_is_authenticated():
            return
        messages = [{"cmd": "LocationChecks", "locations": sorted(self.locations_checked)}]
        if self.finished_game:
            messages.append({"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL})
        await self.send_msgs(messages)

    def server_is_authenticated(self):
        return self.server is not None and self.slot is not None

    async def attach_game(self, writer):
        previous = self.game_writer
        self.game_writer = writer
        self.game_items_sent = 0
        if previous is not None and previous is not writer:
            try:
                previous.close()
                await previous.wait_closed()
            except (ConnectionError, OSError):
                pass

        self.send_to_game(
            "welcome", game=GAME_ID, version=PROTOCOL_VERSION,
            host=LOCAL_HOST, port=LOCAL_PORT)
        self.push_game_snapshot()

    async def detach_game(self, writer):
        if self.game_writer is writer:
            self.game_writer = None
            self.game_items_sent = 0
            logger.info("GTA III disconnected from the local bridge; waiting for it to reconnect.")
        try:
            writer.close()
            await writer.wait_closed()
        except (ConnectionError, OSError):
            pass

    async def close_game_connection(self):
        writer = self.game_writer
        if writer is not None:
            await self.detach_game(writer)

    def send_to_game(self, message_type, **fields):
        writer = self.game_writer
        if writer is None:
            return False
        try:
            writer.write(encode_message(message_type, **fields))
        except (ConnectionError, OSError):
            if writer is self.game_writer:
                self.game_writer = None
            return False
        self.start_task(self._drain_game_writer(writer))
        return True

    async def _drain_game_writer(self, writer):
        try:
            await writer.drain()
        except (ConnectionError, OSError):
            if writer is self.game_writer:
                self.game_writer = None

    def push_pending_items(self):
        if self.game_writer is None:
            return
        if self.game_items_sent > len(self.items_received):
            self.game_items_sent = 0
        for index in range(self.game_items_sent, len(self.items_received)):
            item = self.items_received[index]
            item_name = ITEM_NAME_BY_ID.get(item.item)
            if item_name is None:
                logger.warning("Cannot forward unknown GTA III item id %s", item.item)
                return  # Preserve the cursor: later items must not create a gap.
            if not self.send_to_game(
                    "item", index=index, item=item_name, item_id=item.item):
                return
            self.game_items_sent = index + 1

    def push_checked_locations(self, location_ids=None):
        if self.game_writer is None:
            return
        ids = (self.checked_locations | self.locations_checked
               if location_ids is None else set(location_ids))
        for location_id in sorted(ids):
            location_name = LOC_NAME_BY_ID.get(location_id)
            if location_name is not None:
                self.send_to_game(
                    "checked", location=location_name, location_id=location_id)

    def push_game_snapshot(self):
        if self.game_writer is None:
            return
        authenticated = self.server_is_authenticated()
        self.send_to_game("status", server_connected=authenticated)
        # No snapshot before authentication: an empty item list would make the ASI replay consumables.
        if not authenticated:
            return
        self.send_to_game(
            "room", seed=self.seed_name or "", slot=self.auth or "")
        self.send_to_game("sync_start")
        self.push_checked_locations()
        self.push_pending_items()
        self.send_to_game("sync_end")

    async def accept_game_check(self, location_name):
        location_id = LOC_ID_BY_NAME.get(location_name)
        if location_id is None:
            self.send_to_game(
                "error", code="unknown_location", location=location_name)
            return

        already_checked = (
            location_id in self.checked_locations or
            location_id in self.locations_checked)
        self.locations_checked.add(location_id)

        messages = [{"cmd": "LocationChecks", "locations": [location_id]}]
        if location_name == data["victory_location"]:
            self.finished_game = True
            messages.append({
                "cmd": "StatusUpdate",
                "status": ClientStatus.CLIENT_GOAL,
            })
        if self.server_is_authenticated():
            await self.send_msgs(messages)
        self.send_to_game(
            "check_ack", location=location_name,
            state="already_checked" if already_checked else "queued")

    async def report_game_goal(self):
        self.finished_game = True
        if self.server_is_authenticated():
            await self.send_msgs([{
                "cmd": "StatusUpdate",
                "status": ClientStatus.CLIENT_GOAL,
            }])
        self.send_to_game("goal_ack")

    def on_package(self, cmd, args):
        super().on_package(cmd, args)
        if cmd == "Connected":
            identity = (self.seed_name, self.auth, self.team, self.slot)
            if self._room_identity is not None and identity != self._room_identity:
                self.locations_checked.clear()
                self.finished_game = False
            self._room_identity = identity
            self.game_items_sent = 0
            self.push_game_snapshot()
            self.start_task(self.resend_progress())
        elif cmd == "ReceivedItems":
            self.push_pending_items()
        elif cmd == "RoomUpdate":
            self.push_checked_locations(args.get("checked_locations", []))

    async def connection_closed(self):
        await super().connection_closed()
        self.game_items_sent = 0
        self.send_to_game("status", server_connected=False)

    async def server_auth(self, password_requested: bool = False):
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        # CommonClient replays progress before dispatching Connected to us.
        # Clear another slot's pending progress before that replay can happen.
        if (self._room_identity is not None and
                (self.seed_name, self.auth) != self._room_identity[:2]):
            self.locations_checked.clear()
            self.checked_locations.clear()
            self.finished_game = False
        await self.send_connect()


async def handle_game_connection(reader, writer, ctx):
    peer = writer.get_extra_info("peername")
    attached = False
    try:
        raw_line = await asyncio.wait_for(reader.readline(), timeout=5)
        hello = decode_game_message(raw_line)
        if hello["type"] != "hello":
            raise ProtocolError("the first message must be hello")

        await ctx.attach_game(writer)
        attached = True
        logger.info("GTA III connected to the local bridge from %s.", peer)

        while True:
            raw_line = await reader.readline()
            if not raw_line:
                break
            message = decode_game_message(raw_line)
            message_type = message["type"]
            if message_type == "check":
                await ctx.accept_game_check(message["location"])
            elif message_type == "goal":
                await ctx.report_game_goal()
            elif message_type == "ping":
                ctx.send_to_game("pong")
            else:
                raise ProtocolError("hello may only be sent once")
    except (ProtocolError, asyncio.TimeoutError, ValueError) as exc:
        logger.warning("Rejected local GTA III bridge connection from %s: %s", peer, exc)
        try:
            writer.write(encode_message("error", code="protocol_error", message=str(exc)))
            await writer.drain()
        except (ConnectionError, OSError):
            pass
    except (ConnectionError, OSError):
        pass
    finally:
        if attached:
            await ctx.detach_game(writer)
        else:
            try:
                writer.close()
                await writer.wait_closed()
            except (ConnectionError, OSError):
                pass


def launch(*launch_args):
    async def main(args):
        ctx = GTA3Context(args.connect, args.password)
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
        game_server = None
        try:
            game_server = await asyncio.start_server(
                lambda reader, writer: handle_game_connection(reader, writer, ctx),
                LOCAL_HOST,
                LOCAL_PORT,
                limit=MAX_MESSAGE_BYTES,
            )
            logger.info(
                "GTA III local bridge listening on %s:%s.",
                LOCAL_HOST, LOCAL_PORT)
            if gui_enabled:
                ctx.run_gui()
            ctx.run_cli()
            await ctx.exit_event.wait()
        finally:
            if game_server is not None:
                game_server.close()
                await game_server.wait_closed()
            await ctx.close_game_connection()
            await ctx.shutdown()

    parser = get_base_parser(description="GTA3 Archipelago text client.")
    args = parser.parse_args(launch_args)
    import colorama
    colorama.just_fix_windows_console()
    asyncio.run(main(args))
    colorama.deinit()
