"""Checks GTA3 Client's handling of the room and of the save file, without Archipelago nor the game: CommonClient, Utils and NetUtils are replaced by stubs, the mod by a list of lines.

Usage: python apworld/tools/check_client.py
It checks the lines the mod hears and their order, the save file's name, what is written and when, and what a damaged file or a bad line of the mod gives.
The save files go to a temporary folder, never to Archipelago's."""
import asyncio
import importlib
import json
import logging
import os
import sys
import tempfile
import types
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
saves = tempfile.mkdtemp(prefix="gta3_saves_test_")


class StubContext:
    def __init__(self, server_address, password) -> None:
        self.exit_event = asyncio.Event()
        self.items_received = []
        self.team = None
        self.slot = None
        self.tags = set()
        self.sent = []
        self.item_names = types.SimpleNamespace(lookup_in_slot=lambda item_id: items.ITEM_OF_ID[item_id].name)

    def on_package(self, cmd, args) -> None:
        pass

    async def connection_closed(self) -> None:
        pass

    async def send_msgs(self, messages) -> None:
        self.sent.extend(messages)

    async def update_death_link(self, on: bool) -> None:
        pass


common = types.ModuleType("CommonClient")
common.CommonContext = StubContext
# The client's warnings about the bad lines this check sends on purpose are not shown.
common.logger = logging.getLogger("check_client")
common.logger.setLevel(logging.ERROR)
common.get_base_parser = common.handle_url_arg = common.server_loop = None
common.gui_enabled = False
utils = types.ModuleType("Utils")
utils.user_path = lambda *parts: os.path.join(saves, *parts[1:]) if parts and parts[0] == "gta3_saves" else os.path.join(saves, *parts)
utils.async_start = lambda coroutine, name=None: asyncio.ensure_future(coroutine)
net = types.ModuleType("NetUtils")
net.ClientStatus = types.SimpleNamespace(CLIENT_GOAL=30)
sys.modules.update({"CommonClient": common, "Utils": utils, "NetUtils": net})
package = types.ModuleType("gta3")
package.__path__ = [str(repo / "apworld" / "gta3")]
sys.modules["gta3"] = package
items = importlib.import_module("gta3.items")
client = importlib.import_module("gta3.client")

failures = 0


def check(what: str, ok: bool, detail: object = "") -> None:
    global failures
    failures += not ok
    print("ok  " if ok else "FAIL", what, "" if ok else f"-> {detail}")


def session(seed: str = "1234", slot: int = 1, received: list[int] = ()) -> tuple[object, list[str]]:
    """A client connected to a room of this seed, as our slot, with these item ids received; returns it with the lines it sent the mod."""
    context = client.GTA3Context(None, None)
    lines: list[str] = []
    context.mod.send = lines.append
    context.on_package("RoomInfo", {"seed_name": seed})
    context.team, context.slot = 0, slot
    context.on_package("Connected", {"slot_data": {"need_money": False, "weapon_items": [3, 4], "death_link": False}})
    context.items_received = [types.SimpleNamespace(item=item_id) for item_id in received]
    context.on_package("ReceivedItems", {"index": 0})
    return context, lines


async def main() -> None:
    uzi, money, uzi_license = items.ITEM_NAMED["Uzi"].id, items.ITEM_NAMED["$10000"].id, items.ITEM_NAMED["Uzi License"].id
    context, lines = session(received=[money, uzi])
    check("a new slot: what the mod hears, in order",
          lines == ["consumables give_weapon money", "given_index 0", "player", "option need_money 0", "weapons 3 4", "status connected",
                    "item 0 money 10000 $10000", "item 1 give_weapon 3 Uzi"], lines)
    save = Path(context.save_file)
    check("the save file is named after the seed, the team and the slot", save.name == "1234_0_1.json", save.name)
    check("nothing is written before the mod reports", not save.exists())

    await context.on_mod_progress(2, {"money": 10000, "health": 100, "armour": 0, "in_hand": 3, "weapons": [[3, 125]]})
    written = json.loads(save.read_text(encoding="utf-8"))
    check("a progress line is written whole", written == {"given_index": 2, "player": {"money": 10000, "health": 100, "armour": 0, "in_hand": 3, "weapons": [[3, 125]]}}, written)
    stamp = save.stat().st_mtime_ns
    await context.on_mod_progress(2, {"money": 10000, "health": 100, "armour": 0, "in_hand": 3, "weapons": [[3, 125]]})
    check("the same progress again writes nothing", save.stat().st_mtime_ns == stamp)
    await context.on_mod_progress(1, {"money": 9200, "health": 80, "armour": 0, "in_hand": 3, "weapons": [[3, 125]]})
    written = json.loads(save.read_text(encoding="utf-8"))
    check("given_index never goes back, the state follows the game", written["given_index"] == 2 and written["player"]["money"] == 9200, written)
    check("no temporary file is left", not Path(str(save) + ".tmp").exists())
    await context.connection_closed()
    check("the room lost: the client forgets the slot", context.kept is None)

    context, lines = session(received=[money, uzi, uzi_license])
    check("the next session sends what the file kept, before the items",
          lines[:6] == ["consumables give_weapon money", "given_index 2", "player money=9200 health=80 armour=0 in_hand=3 weapon=3:125", "option need_money 0", "weapons 3 4", "status connected"],
          lines[:6])
    check("and every item again, from index 0", lines[6:] == ["item 0 money 10000 $10000", "item 1 give_weapon 3 Uzi", "item 2 weapon 3 Uzi License"], lines[6:])

    other, other_lines = session(seed="9999")
    check("another room starts from nothing", "given_index 0" in other_lines and "player" in other_lines, other_lines[:3])
    second_slot, second_lines = session(slot=2)
    check("another slot of the same room starts from nothing", "given_index 0" in second_lines and Path(second_slot.save_file).name == "1234_0_2.json", second_lines[:3])

    save.write_text('{"given_index": -4, "player": {"money": "lots", "health": 50, "weapons": [[3, 10], [99, 1], [4]]}}', encoding="utf-8")
    context, lines = session()
    check("a damaged file keeps only valid values", lines[1:3] == ["given_index 0", "player health=50 weapon=3:10"], lines[1:3])
    save.write_text("not json", encoding="utf-8")
    context, lines = session()
    check("an unreadable file counts as nothing saved", lines[1:3] == ["given_index 0", "player"], lines[1:3])

    lines.clear()
    await context.mod.handle_line("progress given_index=3 money=50 weapon=3:7")
    check("a progress line from the mod reaches the save file", json.loads(save.read_text(encoding="utf-8")) == {"given_index": 3, "player": {"money": 50, "weapons": [[3, 7]]}})
    for bad in ("progress money=50", "progress given_index=3 money=-5", "progress given_index=3 speed=9", "progress given_index=x"):
        before = save.read_text(encoding="utf-8")
        await context.mod.handle_line(bad)
        check(f"a bad line is ignored: {bad!r}", save.read_text(encoding="utf-8") == before)


asyncio.run(main())
print(f"--- {failures} failure(s)")
sys.exit(1 if failures else 0)
