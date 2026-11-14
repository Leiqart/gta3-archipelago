"""Generates GTA III rooms for every combination of the options and checks each spoiler: what fills the 73 locations, and what the player starts with.
Then excluded locations (each takes the place of a license; more than 9 cannot be filled by GTA III alone), and a few multiworld rooms:
GTA III with A Short Hike (shipped with Archipelago), and two GTA III slots in one room.

Usage: python apworld/tools/check_generation.py
Install the apworld first (apworld/build.cmd): the generator uses the one in custom_worlds. Archipelago is ARCHIPELAGO_DIR, else C:/ProgramData/Archipelago.
The rooms go to apworld/output/check_generation. It takes a few minutes, one generation after the other."""
import itertools
import os
import re
import shutil
import subprocess
import sys
import types
import zipfile
from collections import Counter
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
archipelago = Path(os.environ.get("ARCHIPELAGO_DIR") or r"C:\ProgramData\Archipelago")
work = repo / "apworld" / "output" / "check_generation"
missions = types.ModuleType("missions")
exec(compile((repo / "apworld/gta3/missions.py").read_text(encoding="utf-8"), "missions.py", "exec"), missions.__dict__)
LOCATIONS = sorted((mission.name for mission in missions.ALL_MISSIONS), key=len, reverse=True)
items = types.ModuleType("gta3.items")
# items.py imports missions.py as a relative module: it gets the one read above.
sys.modules["gta3"] = types.ModuleType("gta3")
sys.modules["gta3"].__path__ = [str(repo / "apworld/gta3")]
sys.modules["gta3.missions"] = missions
items.__package__ = "gta3"
exec(compile((repo / "apworld/gta3/items.py").read_text(encoding="utf-8"), "items.py", "exec"), items.__dict__)
GIFTS = {gift.name for gift in items.GIFTS}
failures = []


def kind(item: str) -> str:
    if item == "$10000":
        return "money"
    if item.startswith("Progressive "):
        return "progressive"
    if item.endswith(" Unlock"):
        return "unlock"
    if item.endswith(" License"):
        return "license"
    return "gift" if item in GIFTS else "other"


def generate(name: str, yamls: dict[str, str], seed: int) -> tuple[int, str, str]:
    folder = work / name
    if folder.exists():
        shutil.rmtree(folder)
    (folder / "players").mkdir(parents=True)
    for file, text in yamls.items():
        (folder / "players" / file).write_text(text, encoding="utf-8")
    run = subprocess.run([str(archipelago / "ArchipelagoGenerate.exe"), "--player_files_path", str(folder / "players"), "--outputpath", str(folder), "--spoiler", "2", "--seed", str(seed)],
                         stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding="utf-8", errors="replace")
    rooms = list(folder.glob("AP_*.zip"))
    spoiler = ""
    if rooms:
        archive = zipfile.ZipFile(rooms[0])
        spoiler = archive.read(next(n for n in archive.namelist() if n.endswith("_Spoiler.txt"))).decode("utf-8").replace("\r\n", "\n")
    return run.returncode if rooms else (run.returncode or 1), spoiler, run.stdout + run.stderr


def gta_yaml(name: str, order: str, need_money: bool, death_link: bool, start: dict[str, int], excluded: list[str] = ()) -> str:
    lines = [f"name: {name}", "game: Grand Theft Auto III", "Grand Theft Auto III:", f"  mission_order: {order}", f"  need_money: {str(need_money).lower()}",
             f"  death_link: {str(death_link).lower()}", f"  arrest_is_death: {str(not death_link).lower()}"]
    if start:
        lines += ["  start_inventory_from_pool:"] + [f"    {item}: {count}" for item, count in start.items()]
    if excluded:
        lines += ["  exclude_locations:"] + [f"    - {location}" for location in excluded]
    return "\n".join(lines) + "\n"


def placed_solo(spoiler: str) -> dict[str, str]:
    section = spoiler.split("\nLocations:\n\n", 1)[1].split("\n\n", 1)[0]
    placed = {}
    for line in section.splitlines():
        location = next((name for name in LOCATIONS if line.startswith(name + ": ")), None)
        if location:
            placed[location] = line[len(location) + 2:].strip()
    return placed


def check(what: str, ok: bool, detail: str = "") -> None:
    print(("ok  " if ok else "FAIL"), what, detail)
    if not ok:
        failures.append(what)


# --- Solo rooms: every option, and each kind of item taken from the pool.
starts = {"nothing": {}, "a license": {"Uzi License": 1}, "an unlock": None, "a weapon gift": {"Uzi": 1}, "money": {"$10000": 1}}
seed = 1000
for order, need_money, death_link, (start_name, start) in itertools.product(("ordered", "free"), (False, True), (False, True), starts.items()):
    seed += 1
    if start is None:
        start = {"Progressive Luigi Mission": 1} if order == "ordered" else {"Don't Spank Ma Bitch Up Unlock": 1}
    name = f"{order}-money{int(need_money)}-dl{int(death_link)}-{start_name.replace(' ', '_')}"
    code, spoiler, output = generate(name, {"GTA3.yaml": gta_yaml("Leiqart", order, need_money, death_link, start)}, seed)
    if code != 0:
        check(name, False, "generation failed: " + output.strip().splitlines()[-1][:200])
        continue
    kinds = Counter(kind(item) for item in placed_solo(spoiler).values())
    unlocks = "progressive" if order == "ordered" else "unlock"
    removed_unlock = int(start_name == "an unlock")
    removed_license = int(start_name == "a license")
    expected = {unlocks: 64 - removed_unlock, "license": 9 - removed_license}
    if removed_unlock + removed_license:
        expected["money"] = removed_unlock + removed_license
    starting = re.search(r"Start Inventory from Pool:\s*(.*)", spoiler)
    check(name, dict(kinds) == expected, f"placed {dict(kinds)}, start from pool: {starting.group(1).strip() if starting else '-'}")

# --- Excluded locations: each one takes the place of a license, so that a $10000 goes there; past 9, a room of GTA III alone cannot fill them.
excluded = ["Multistorey Mayhem", "Gripped!"]
code, spoiler, output = generate("solo-excluded", {"GTA3.yaml": gta_yaml("Leiqart", "ordered", False, False, {}, excluded)}, 2001)
placed = placed_solo(spoiler) if code == 0 else {}
check("solo room with 2 excluded locations: a $10000 on each, 7 licenses left",
      code == 0 and all(placed.get(location) == "$10000" for location in excluded) and Counter(kind(item) for item in placed.values())["license"] == 7,
      {location: placed.get(location) for location in excluded} if code == 0 else output.strip().splitlines()[-1][:160])
many = [mission.name for mission in missions.SIDE_MISSIONS] + ["The Crook", "The Thieves"]
code, spoiler, output = generate("solo-excluded-10", {"GTA3.yaml": gta_yaml("Leiqart", "free", False, False, {}, many)}, 2005)
check(f"solo room with {len(many)} excluded locations, more than the 9 the unlocks leave, cannot be filled", code != 0, "it generated" if code == 0 else "")
hike = "name: Hiker\ngame: A Short Hike\nA Short Hike: {}\n"
code, spoiler, output = generate("with-hike-excluded", {"GTA3.yaml": gta_yaml("Leiqart", "ordered", False, False, {}, excluded), "Hike.yaml": hike}, 2002)
lines = [line.strip() for line in spoiler.splitlines() if any(line.startswith(f"{location} (Leiqart): ") for location in excluded)]
check("with A Short Hike, 2 excluded locations get its filler", code == 0 and len(lines) == 2, "; ".join(lines) if code == 0 else output.strip().splitlines()[-1][:160])

# --- Multiworld: GTA III with A Short Hike, then two GTA III slots in one room.
code, spoiler, output = generate("with-hike", {"GTA3.yaml": gta_yaml("Leiqart", "free", True, True, {"Uzi License": 1}), "Hike.yaml": hike}, 2003)
gta_items = [line for line in spoiler.splitlines() if re.search(r"\(Leiqart\)$", line.strip())]
check("GTA III with A Short Hike", code == 0, f"{len(gta_items)} items of Leiqart placed in the multiworld" if code == 0 else output.strip().splitlines()[-1][:160])
code, spoiler, output = generate("two-gta", {"GTA3a.yaml": gta_yaml("SlotOne", "ordered", False, False, {}), "GTA3b.yaml": gta_yaml("SlotTwo", "free", False, False, {})}, 2004)
check("two GTA III slots in one room", code == 0, "" if code == 0 else output.strip().splitlines()[-1][:160])

print(f"--- {len(failures)} failure(s)" + (": " + ", ".join(failures) if failures else ""))
sys.exit(1 if failures else 0)
