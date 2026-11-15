"""Generates GTA III rooms for every combination of the options and checks each spoiler: what fills the 73 locations, and what the player starts with.
Then excluded locations (each takes the place of a license; more than the locations the unlocks leave cannot be filled by GTA III alone), and a few multiworld rooms:
GTA III with A Short Hike (shipped with Archipelago), and two GTA III slots in one room.

Usage: python apworld/tools/check_generation.py
Install the apworld first (apworld/build.cmd): the generator uses the one in custom_worlds. Archipelago is ARCHIPELAGO_DIR, else C:/ProgramData/Archipelago.
The rooms go to apworld/output/check_generation. It takes a few minutes, one generation after the other."""
import importlib
import itertools
import re
import sys
import types
import zipfile
from collections import Counter

from tooling import REPO, Checks, generate

WORK = REPO / "apworld" / "output" / "check_generation"
# The apworld's tables, without Archipelago: a stub package keeps gta3/__init__.py, which needs it, from running.
sys.modules["gta3"] = types.ModuleType("gta3")
sys.modules["gta3"].__path__ = [str(REPO / "apworld" / "gta3")]
missions = importlib.import_module("gta3.missions")
items = importlib.import_module("gta3.items")
# Longest names first: "Bomb Da Base: Act II" before "Bomb Da Base: Act I", as both names and items hold ": ".
LOCATIONS = sorted((mission.name for mission in missions.ALL_MISSIONS), key=len, reverse=True)
UNLOCKS = [mission for mission in missions.MISSIONS if mission.unlock_id]
KINDS = {
    items.MONEY.name: "money",
    **{contact.progressive_item: "progressive" for contact in missions.CONTACTS},
    **{missions.unlock_item_name(mission): "unlock" for mission in UNLOCKS},
    **{weapon.name: "license" for weapon in items.WEAPONS},
    **{gift.name: "gift" for gift in items.GIFTS},
}
checks = Checks()


# Generates a room from these YAML files, by name; returns the generator's exit code, the room's spoiler ("" without a room) and what the generator wrote.
def room(name: str, yamls: dict[str, str], seed: int) -> tuple[int, str, str]:
    players = WORK / name / "players"
    players.mkdir(parents=True, exist_ok=True)
    for file, text in yamls.items():
        (players / file).write_text(text, encoding="utf-8")
    code, archive, output = generate(WORK / name / "room", players, seed)
    if archive is None:
        return code, "", output
    with zipfile.ZipFile(archive) as files:
        spoiler = files.read(next(name for name in files.namelist() if name.endswith("_Spoiler.txt"))).decode("utf-8").replace("\r\n", "\n")
    return code, spoiler, output


def gta_yaml(name: str, order: str, need_money: bool, death_link: bool, start: dict[str, int], excluded: list[str] = ()) -> str:
    lines = [f"name: {name}", "game: Grand Theft Auto III", "Grand Theft Auto III:", f"  mission_order: {order}", f"  need_money: {str(need_money).lower()}",
             f"  death_link: {str(death_link).lower()}", f"  arrest_is_death: {str(not death_link).lower()}"]
    if start:
        lines += ["  start_inventory_from_pool:"] + [f"    {item}: {count}" for item, count in start.items()]
    if excluded:
        lines += ["  exclude_locations:"] + [f"    - {location}" for location in excluded]
    return "\n".join(lines) + "\n"


# What a solo room's spoiler puts on each of our locations.
def placed_solo(spoiler: str) -> dict[str, str]:
    section = spoiler.split("\nLocations:\n\n", 1)[1].split("\n\n", 1)[0]
    placed = {}
    for line in section.splitlines():
        location = next((name for name in LOCATIONS if line.startswith(name + ": ")), None)
        if location:
            placed[location] = line[len(location) + 2:].strip()
    return placed


# How many items of each kind (see KINDS) a solo room puts on our locations.
def kinds_placed(spoiler: str) -> dict[str, int]:
    return dict(Counter(KINDS.get(item, "other") for item in placed_solo(spoiler).values()))


def last_line(output: str) -> str:
    return output.strip().splitlines()[-1][:200] if output.strip() else ""


# --- Solo rooms: every option, and each kind of item taken from the pool.
starts = {"nothing": {}, "a license": {"Uzi License": 1}, "an unlock": None, "a weapon gift": {"Uzi": 1}, "money": {items.MONEY.name: 1}}
seed = 1000
for order, need_money, death_link, (start_name, start) in itertools.product(("ordered", "free"), (False, True), (False, True), starts.items()):
    seed += 1
    if start is None:
        start = {missions.CONTACTS[0].progressive_item: 1} if order == "ordered" else {missions.unlock_item_name(UNLOCKS[0]): 1}
    name = f"{order}-money{int(need_money)}-dl{int(death_link)}-{start_name.replace(' ', '_')}"
    code, spoiler, output = room(name, {"GTA3.yaml": gta_yaml("Leiqart", order, need_money, death_link, start)}, seed)
    if code != 0:
        checks.check(name, False, "generation failed: " + last_line(output))
        continue
    # An item taken from the pool leaves a $10000 in its place; one that is not in the pool (a gift, money) takes nothing out.
    removed_unlock, removed_license = int(start_name == "an unlock"), int(start_name == "a license")
    expected = {"progressive" if order == "ordered" else "unlock": len(UNLOCKS) - removed_unlock, "license": len(items.WEAPONS) - removed_license}
    if removed_unlock + removed_license:
        expected["money"] = removed_unlock + removed_license
    starting = re.search(r"Start Inventory from Pool:\s*(.*)", spoiler)
    checks.check(name, kinds_placed(spoiler) == expected, f"placed {kinds_placed(spoiler)}, start from pool: {starting.group(1).strip() if starting else '-'}")

# --- Excluded locations: each one takes the place of a license, so that a $10000 goes there; past the locations the unlocks leave, GTA III alone cannot fill them.
excluded = ["Multistorey Mayhem", "Gripped!"]
code, spoiler, output = room("solo-excluded", {"GTA3.yaml": gta_yaml("Leiqart", "ordered", False, False, {}, excluded)}, 2001)
placed = placed_solo(spoiler) if code == 0 else {}
checks.check(f"solo room with 2 excluded locations: a $10000 on each, {len(items.WEAPONS) - 2} licenses left",
             code == 0 and all(placed.get(location) == items.MONEY.name for location in excluded) and kinds_placed(spoiler).get("license") == len(items.WEAPONS) - 2,
             {location: placed.get(location) for location in excluded} if code == 0 else last_line(output))
many = [mission.name for mission in missions.SIDE_MISSIONS] + ["The Crook", "The Thieves"]
code, spoiler, output = room("solo-excluded-many", {"GTA3.yaml": gta_yaml("Leiqart", "free", False, False, {}, many)}, 2005)
checks.check(f"solo room with {len(many)} excluded locations, more than the {len(missions.ALL_MISSIONS) - len(UNLOCKS)} the unlocks leave, cannot be filled",
             code != 0, "it generated")
hike = "name: Hiker\ngame: A Short Hike\nA Short Hike: {}\n"
code, spoiler, output = room("with-hike-excluded", {"GTA3.yaml": gta_yaml("Leiqart", "ordered", False, False, {}, excluded), "Hike.yaml": hike}, 2002)
lines = [line.strip() for line in spoiler.splitlines() if any(line.startswith(f"{location} (Leiqart): ") for location in excluded)]
checks.check("with A Short Hike, 2 excluded locations get filler", code == 0 and len(lines) == 2, "; ".join(lines) if code == 0 else last_line(output))

# --- Multiworld: GTA III with A Short Hike, then two GTA III slots in one room.
code, spoiler, output = room("with-hike", {"GTA3.yaml": gta_yaml("Leiqart", "free", True, True, {"Uzi License": 1}), "Hike.yaml": hike}, 2003)
checks.check("GTA III with A Short Hike", code == 0, last_line(output))
code, spoiler, output = room("two-gta", {"GTA3a.yaml": gta_yaml("SlotOne", "ordered", False, False, {}), "GTA3b.yaml": gta_yaml("SlotTwo", "free", False, False, {})}, 2004)
checks.check("two GTA III slots in one room", code == 0, last_line(output))

sys.exit(checks.sum_up())
