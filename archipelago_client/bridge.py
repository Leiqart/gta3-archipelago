#!/usr/bin/env python3
"""GTA3 Archipelago bridge — networked client (Bloc D).

Connects an Archipelago room (archipelago.gg or a local ArchipelagoServer) to the
running game through the plugin's file channel:

  server --(ReceivedItems)-->  bridge  --writes-->  III.Archipelago.items.json
                                                     (plugin applies unlock_*)

  plugin --writes--> III.Archipelago.state.json (locations_checked)
                                  |
                                  v
  bridge --reads, maps, sends--> server (LocationChecks)

Item / location names map 1:1 to the ids our apworld generated (gta3/data.py),
so the bridge needs no datapackage round-trip.

Usage:
    python bridge.py --server archipelago.gg:54321 --slot GTA3Player
    python bridge.py                      # defaults: localhost:38281, GTA3Player

Run it while GTA III is running; it keeps syncing until you Ctrl-C.
"""
import argparse
import asyncio
import json
import os
import sys
import uuid

try:
    import websockets
except ImportError:
    print("Missing dependency: pip install websockets", file=sys.stderr)
    raise

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_GAME_DIR = os.path.normpath(os.path.join(HERE, "..", "Grand Theft Auto 3 -moded"))
GAME_DIR = DEFAULT_GAME_DIR
STATE_FILE = os.path.join(GAME_DIR, "III.Archipelago.state.json")    # plugin -> bridge
ITEMS_FILE = os.path.join(GAME_DIR, "III.Archipelago.items.json")    # bridge -> plugin
BRIDGE_FILE = os.path.join(GAME_DIR, "III.Archipelago.bridge.json")  # bridge -> plugin (toast)
STATUS_FILE = os.path.join(GAME_DIR, "III.Archipelago.status.json")  # bridge -> plugin (connection)
RUNSTATE_INI = os.path.join(GAME_DIR, "III.MissionSelector.state.ini")  # plugin <-> server (data storage)
SYNC_FILE = os.path.join(GAME_DIR, "III.Archipelago.sync.json")      # bridge -> plugin (reload signal)
ROOM_FILE = os.path.join(GAME_DIR, "III.Archipelago.room.json")      # bridge memory: last room seed
RUNSTATE_KEY_PREFIX = "gta3_runstate_"

GAME_NAME = "Grand Theft Auto III"
AP_VERSION = {"major": 0, "minor": 6, "build": 6, "class": "Version"}

# id <-> name maps from the apworld data (same ids the server uses). Load data.py
# directly so we don't trigger gta3/__init__.py (which imports the AP framework,
# unavailable in the plain system Python).
import importlib.util  # noqa: E402
_spec = importlib.util.spec_from_file_location(
    "gta3_data", os.path.join(HERE, "gta3", "data.py"))
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
data = _mod.data

ITEM_ID_TO_NAME = {v: k for k, v in data["item_ids"].items()}
LOC_NAME_TO_ID = dict(data["location_ids"])
LOC_ID_TO_NAME = {v: k for k, v in data["location_ids"].items()}


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def set_game_dir(path):
    global GAME_DIR, STATE_FILE, ITEMS_FILE, BRIDGE_FILE, STATUS_FILE
    global RUNSTATE_INI, SYNC_FILE, ROOM_FILE
    GAME_DIR = os.path.normpath(path)
    STATE_FILE = os.path.join(GAME_DIR, "III.Archipelago.state.json")
    ITEMS_FILE = os.path.join(GAME_DIR, "III.Archipelago.items.json")
    BRIDGE_FILE = os.path.join(GAME_DIR, "III.Archipelago.bridge.json")
    STATUS_FILE = os.path.join(GAME_DIR, "III.Archipelago.status.json")
    RUNSTATE_INI = os.path.join(GAME_DIR, "III.MissionSelector.state.ini")
    SYNC_FILE = os.path.join(GAME_DIR, "III.Archipelago.sync.json")
    ROOM_FILE = os.path.join(GAME_DIR, "III.Archipelago.room.json")


def load_ap_connexion(game_dir):
    cfg = {}
    path = os.path.join(game_dir, "AP_connexion.txt")
    try:
        with open(path, "r", encoding="utf-8") as f:
            for raw in f:
                line = raw.split(";", 1)[0].strip()
                if "=" not in line:
                    continue
                key, value = line.split("=", 1)
                key = key.strip().lower()
                value = value.strip()
                if key in ("ap_server", "server", "address") and value:
                    cfg["server"] = value
                elif key in ("ap_slot", "slot", "pseudo", "name") and value:
                    cfg["slot"] = value
                elif key in ("ap_password", "password"):
                    cfg["password"] = value
    except OSError:
        pass
    return cfg


def load_items_state():
    data = load_json(ITEMS_FILE)
    received = data.get("received_items", [])
    raw = data.get("received_items_raw", received)
    if not isinstance(received, list):
        received = []
    if not isinstance(raw, list):
        raw = list(received)
    clean_received = [name for name in received if isinstance(name, str)]
    clean_raw = [name if isinstance(name, str) else None for name in raw]
    return clean_received, clean_raw


def write_items(names, raw_names=None):
    """Bridge owns III.Archipelago.items.json (received_items)."""
    payload = {"received_items": names}
    payload["received_items_raw"] = raw_names if raw_names is not None else list(names)
    with open(ITEMS_FILE, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)


def toast(msg):
    """Pop a one-shot message on screen via the plugin's bridge.json channel."""
    seq = int(load_json(BRIDGE_FILE).get("seq", 0)) + 1
    with open(BRIDGE_FILE, "w", encoding="utf-8") as f:
        json.dump({"seq": seq, "toast": msg}, f)


def write_status(connected, slot="", server=""):
    """Persistent connection state the plugin shows in the start menu."""
    try:
        with open(STATUS_FILE, "w", encoding="utf-8") as f:
            json.dump({"connected": bool(connected), "slot": slot, "server": server}, f)
    except Exception:
        pass


def read_runstate_ini():
    try:
        with open(RUNSTATE_INI, "r", encoding="utf-8") as f:
            text = f.read()
    except OSError:
        return ""
    sanitized = sanitize_runstate_ini(text)
    if sanitized != text:
        try:
            with open(RUNSTATE_INI, "w", encoding="utf-8", newline="") as f:
                f.write(sanitized)
        except OSError:
            pass
    return sanitized


def runstate_ini_mtime():
    try:
        return os.path.getmtime(RUNSTATE_INI)
    except OSError:
        return 0.0


def write_runstate_ini(text):
    with open(RUNSTATE_INI, "w", encoding="utf-8", newline="") as f:
        f.write(sanitize_runstate_ini(text))


def delete_runstate_ini():
    try:
        os.remove(RUNSTATE_INI)
        return True
    except OSError:
        return False


def sanitize_runstate_ini(text):
    """Keep player persistence, but never persist AP mission validation locally.

    Mission checks are server-owned Archipelago locations. Keeping validated*
    in the local/data-storage INI can resurrect old Give Me Liberty / Luigi
    progress on a fresh launch before the server says those locations are
    actually checked.
    """
    blocked = {"validated", "validated_extra"}
    out = []
    seen = set()
    for raw in (text or "").splitlines():
        if "=" not in raw:
            out.append(raw)
            continue
        key, _value = raw.split("=", 1)
        key = key.strip()
        if key in blocked:
            out.append(f"{key}=")
            seen.add(key)
        else:
            out.append(raw)
    for key in sorted(blocked - seen):
        out.append(f"{key}=")
    return "\n".join(out) + ("\n" if out else "")


def parse_runstate_ini(text):
    state = {}
    for raw in (text or "").splitlines():
        if "=" not in raw:
            continue
        key, value = raw.split("=", 1)
        state[key.strip()] = value.strip()
    return state


def parse_position(value):
    try:
        parts = [float(part.strip()) for part in str(value).split(",")]
    except Exception:
        return None
    if len(parts) != 3:
        return None
    return {"x": parts[0], "y": parts[1], "z": parts[2]}


def parse_weapons(value):
    weapons = []
    for slot, token in enumerate(str(value or "").split(",")):
        if ":" not in token:
            continue
        type_text, ammo_text = token.split(":", 1)
        weapon_type = parse_int(type_text.strip(), 0)
        ammo = parse_int(ammo_text.strip(), 0)
        weapons.append({"slot": slot, "type": weapon_type, "ammo": ammo})
    return weapons


def build_runstate_payload(text):
    text = sanitize_runstate_ini(text)
    parsed = parse_runstate_ini(text)
    player_state = {
        "money": parse_int(parsed.get("money", 0), 0),
        "weapons": parse_weapons(parsed.get("weapons", "")),
        "health": parse_int(parsed.get("health", 0), 0),
        "armor": parse_int(parsed.get("armor", 0), 0),
        "position": parse_position(parsed.get("pos", "")),
        "packages": parse_int(parsed.get("packages", 0), 0),
    }
    return {
        "kind": "gta3_runstate",
        "schema_version": 2,
        "mtime": runstate_ini_mtime(),
        "ini": text,
        # Structured mirror for data-storage inspection/tools. The plugin still
        # reloads the ini blob, which is the durable compatibility format.
        "player_state": player_state,
        "progress": {
            "run_active": parse_int(parsed.get("run_active", 0), 0) != 0,
            "checks": parse_int(parsed.get("checks", 0), 0),
            "map_state": parse_int(parsed.get("map_state", 0), 0),
            "unlocked_chars": parse_int(parsed.get("unlocked_chars", 1), 1),
            "unlocked_buckets": parsed.get("unlocked_buckets", ""),
            "validated": parsed.get("validated", ""),
            "validated_extra": parsed.get("validated_extra", ""),
        },
    }


def _write_sync(seq, settled):
    with open(SYNC_FILE, "w", encoding="utf-8") as f:
        json.dump({"seq": seq, "settled": settled}, f)


def bump_state_sync_seq():
    """Tell the plugin its RunState file was replaced from the server.
    A download also counts as the storage decision being settled."""
    d = load_json(SYNC_FILE)
    _write_sync(int(d.get("seq", 0)) + 1, int(d.get("settled", 0)) + 1)


def mark_storage_settled():
    """The data-storage GET decision is final for this connect (download
    applied, local kept, or nothing stored): the plugin may lift its
    resume loading screen."""
    d = load_json(SYNC_FILE)
    _write_sync(int(d.get("seq", 0)), int(d.get("settled", 0)) + 1)


def reset_local_state_for_new_room(seed_name, slot):
    """The server hosts a DIFFERENT room than the one the local files belong
    to: every locally-stored check, item and run value is stale and would
    contaminate the fresh room (instant bogus checks, polluted data storage).
    Wipe it all and remember the new room's seed."""
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump({
            "schema_version": 1,
            "slot_name": slot,
            "seed_name": seed_name,
            "run_active": True,
            "start_mode": "archipelago",
            "received_items_applied": 0,
            "locations_checked": [],
        }, f, indent=2)
    write_items([], [])
    try:
        os.remove(RUNSTATE_INI)
    except OSError:
        pass
    bump_state_sync_seq()
    with open(ROOM_FILE, "w", encoding="utf-8") as f:
        json.dump({"seed_name": seed_name}, f)
    print(f"  !! new room detected (seed={seed_name}): local run state reset")


def parse_int(value, default=0):
    try:
        return int(value)
    except Exception:
        return default


def write_server_checked_locations(checked_location_ids, slot_name="", seed_name=""):
    """Rewrite local checked-state from the current AP server only.

    The game may append fresh local completions later and the bridge will send
    them. On connect/reconnect, though, old local locations must not be merged
    back in or they become instant bogus LocationChecks.
    """
    state = load_json(STATE_FILE)
    server_locations = set()
    for location_id in checked_location_ids or []:
        loc_name = LOC_ID_TO_NAME.get(location_id)
        if loc_name:
            server_locations.add(loc_name)

    next_state = {
        "schema_version": parse_int(state.get("schema_version", 1), 1),
        "slot_name": slot_name or state.get("slot_name", ""),
        "seed_name": seed_name or state.get("seed_name") or "online",
        "run_active": True,
        "start_mode": state.get("start_mode") or "archipelago",
        "received_items_applied": parse_int(state.get("received_items_applied", 0), 0),
        "locations_checked": sorted(server_locations),
    }
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(next_state, f, indent=2)
    return next_state["locations_checked"]


class GTA3Bridge:
    def __init__(self, server, slot, password):
        self.server = server
        self.slot = slot
        self.password = password
        self.received, self.received_raw = load_items_state()
        self.sent_locations = set()   # location ids already reported
        self.server_checked_locations = set()
        self.supports_give_me_liberty_location = False
        self.connected_announced = False
        # Data-storage run-state sync: the server copy wins at connect (no push
        # until the Get reply lands), then local changes are pushed up.
        self.runstate_key = RUNSTATE_KEY_PREFIX + slot
        self.runstate_storage_ready = False
        self.runstate_last_pushed = None

    async def run(self):
        write_status(False, self.slot, self.server)
        host = self.server
        # Try secure (archipelago.gg) then plain (local) websocket.
        last_err = None
        try:
            for scheme in ("wss", "ws"):
                uri = f"{scheme}://{host}"
                try:
                    async with websockets.connect(uri, max_size=None) as ws:
                        print(f"connected: {uri}")
                        await self.session(ws)
                        return
                except Exception as e:  # noqa: BLE001
                    last_err = e
                    print(f"  {uri} failed: {e}")
            print(f"could not connect to {host}: {last_err}", file=sys.stderr)
        finally:
            write_status(False, self.slot, self.server)

    async def session(self, ws):
        # 1) server greets with RoomInfo
        room_info = await self.recv_until(ws, "RoomInfo")
        # Room-change guard: if this server hosts a different seed/room than
        # the one the local files belong to, reset them BEFORE anything gets
        # pushed (stale locations_checked would register instantly as checks
        # and the old run state would poison the room's data storage).
        seed_name = str(room_info.get("seed_name") or "")
        if seed_name:
            last_seed = str(load_json(ROOM_FILE).get("seed_name") or "")
            if seed_name != last_seed:
                reset_local_state_for_new_room(seed_name, self.slot)
                self.received, self.received_raw = [], []
                self.runstate_last_pushed = None
        # 2) we Connect
        await self.send(ws, [{
            "cmd": "Connect",
            "game": GAME_NAME,
            "name": self.slot,
            "password": self.password,
            "uuid": str(uuid.getnode()),
            "version": AP_VERSION,
            "items_handling": 0b111,   # all remote items
            "tags": [],
            "slot_data": True,
        }])
        msgs = await self.recv(ws)
        pending_msgs = []
        for m in msgs:
            if m["cmd"] == "ConnectionRefused":
                print("connection refused:", m.get("errors"), file=sys.stderr)
                return
            if m["cmd"] == "Connected":
                slot_data = m.get("slot_data") or {}
                self.supports_give_me_liberty_location = bool(
                    slot_data.get("supports_give_me_liberty_location"))
                checked_locations = m.get("checked_locations") or []
                self.server_checked_locations = set(checked_locations)
                self.sent_locations = set(checked_locations)
                synced = write_server_checked_locations(
                    self.server_checked_locations, self.slot, seed_name)
                print(f"  <- synced {len(synced)} checked locations from server")
            else:
                pending_msgs.append(m)
        for m in pending_msgs:
            await self.handle(ws, m)
        print(f"authenticated as {self.slot} ({GAME_NAME})")
        # Pull the run state from data storage before any push.
        await self.send(ws, [{"cmd": "Get", "keys": [self.runstate_key]}])
        # 3) sync loop: pump network + push the game's checks
        await asyncio.gather(self.net_loop(ws), self.checks_loop(ws))

    async def net_loop(self, ws):
        async for raw in ws:
            for m in json.loads(raw):
                await self.handle(ws, m)

    async def handle(self, ws, m):
        cmd = m.get("cmd")
        if cmd == "ReceivedItems":
            start_index = m.get("index")
            next_raw = list(self.received_raw)
            if isinstance(start_index, int) and start_index >= 0:
                if start_index <= len(next_raw):
                    next_raw = next_raw[:start_index]
                else:
                    print(f"  [server] ReceivedItems index {start_index} ahead of local raw count {len(next_raw)}")
            for it in m["items"]:
                name = ITEM_ID_TO_NAME.get(it["item"])
                next_raw.append(name)
                if name == "unlock_luigi":
                    print("  <- item: unlock_luigi (ignored legacy start item)")
                    continue
                if name:
                    print(f"  <- item: {name}")
            next_received = [
                name for name in next_raw
                if isinstance(name, str) and name != "unlock_luigi"
            ]
            if next_raw != self.received_raw or next_received != self.received:
                self.received_raw = next_raw
                self.received = next_received
                write_items(self.received, self.received_raw)
        elif cmd == "PrintJSON":
            txt = "".join(p.get("text", "") for p in m.get("data", []))
            if txt.strip():
                print("  [server]", txt)
            # Relay free chat + server-console text into the game as a toast.
            # Player chat is type "Chat"/"ServerChat"; text typed in the AP
            # server console goes through notify_all -> PrintJSON with NO type.
            # Join/Part/ItemSend/Hint all carry their own type, so excluding the
            # typed ones (keeping only None/Chat/ServerChat) avoids HUD spam.
            if m.get("type") in (None, "Chat", "ServerChat"):
                msg = (m.get("message") or txt).strip()
                if msg:
                    toast(msg[:80])
                    print(f"  -> in-game toast: {msg[:80]}")
        elif cmd == "RoomUpdate":
            checked_locations = m.get("checked_locations")
            if checked_locations:
                self.server_checked_locations.update(checked_locations)
                self.sent_locations.update(checked_locations)
                synced = write_server_checked_locations(
                    self.server_checked_locations, self.slot)
                print(f"  <- room update synced {len(synced)} checked locations")
        elif cmd == "Retrieved":
            keys = m.get("keys") or {}
            value = keys.get(self.runstate_key)
            # Server/DataStorage is authoritative at connect. The local INI
            # is only a live ASI cache; a newer local mtime must not
            # resurrect stale position/cash or skip Give Me Liberty.
            server_ini = ""
            if isinstance(value, dict):
                server_ini = sanitize_runstate_ini(str(value.get("ini") or ""))
            elif isinstance(value, str):
                server_ini = sanitize_runstate_ini(value)  # legacy payload
            local_ini = read_runstate_ini()
            if server_ini:
                if server_ini != local_ini:
                    write_runstate_ini(server_ini)
                    bump_state_sync_seq()
                    print(f"  <- run state downloaded from data storage ({len(server_ini)} bytes, server authoritative)")
                else:
                    mark_storage_settled()
                self.runstate_last_pushed = server_ini
            else:
                if local_ini and delete_runstate_ini():
                    bump_state_sync_seq()
                    print("  <- no run state in data storage; local run state cleared")
                else:
                    mark_storage_settled()
                    print("  <- no run state in data storage")
                self.runstate_last_pushed = ""
            self.runstate_storage_ready = True
            if not self.connected_announced:
                self.connected_announced = True
                write_status(True, self.slot, self.server)
                toast("AP connected")  # confirm the link on screen
        elif cmd == "Bounced":
            pass

    async def checks_loop(self, ws):
        """Read locations the game completed and report them to the server."""
        while True:
            state = load_json(STATE_FILE)
            new_ids = []
            for loc_name in state.get("locations_checked", []):
                if (loc_name == "mission_give_me_liberty" and
                        not self.supports_give_me_liberty_location):
                    continue
                lid = LOC_NAME_TO_ID.get(loc_name)
                if lid is not None and lid not in self.sent_locations:
                    self.sent_locations.add(lid)
                    new_ids.append(lid)
            if new_ids:
                await self.send(ws, [{"cmd": "LocationChecks", "locations": new_ids}])
                print(f"  -> checks: {new_ids}")
            # Run-state upload: push the plugin's RunState file when it changed.
            # Gated on the connect-time Get reply so a fresh local file never
            # clobbers the server-side run.
            if self.runstate_storage_ready:
                runstate = read_runstate_ini()
                if runstate and runstate != self.runstate_last_pushed:
                    await self.send(ws, [{
                        "cmd": "Set",
                        "key": self.runstate_key,
                        "default": {},
                        "want_reply": False,
                        "operations": [{"operation": "replace",
                                        "value": build_runstate_payload(runstate)}],
                    }])
                    self.runstate_last_pushed = runstate
                    print(f"  -> run state pushed to data storage ({len(runstate)} bytes)")
            await asyncio.sleep(1.0)

    async def recv(self, ws):
        return json.loads(await ws.recv())

    async def recv_until(self, ws, cmd):
        while True:
            for m in await self.recv(ws):
                if m["cmd"] == cmd:
                    return m

    async def send(self, ws, payload):
        await ws.send(json.dumps(payload))


def main():
    default_cfg = load_json(os.path.join(HERE, "config.json"))
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", default=DEFAULT_GAME_DIR)
    ap.add_argument("--server")
    ap.add_argument("--slot")
    ap.add_argument("--password")
    args = ap.parse_args()
    set_game_dir(args.game_dir)
    room_cfg = load_ap_connexion(GAME_DIR)
    server = args.server or room_cfg.get("server") or default_cfg.get("server", "localhost:38281")
    slot = args.slot or room_cfg.get("slot") or default_cfg.get("slot", "GTA3Player")
    password = args.password if args.password is not None else room_cfg.get("password", default_cfg.get("password", ""))
    print(f"server={server} slot={slot}")
    if not os.path.isdir(GAME_DIR):
        print(f"game dir not found: {GAME_DIR}", file=sys.stderr)
        return 1
    try:
        asyncio.run(GTA3Bridge(server, slot, password or None).run())
    except KeyboardInterrupt:
        print("\nbridge stopped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
