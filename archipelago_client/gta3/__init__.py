"""Grand Theft Auto III — Archipelago world (MVP).

Locations  = main-story missions (from ARCHIPELAGO_CHECKS.csv).
Items      = unlock_<contact> + unlock_commercial/unlock_suburban (progression),
             plus "cash" filler.
Regions    = Portland (start) / Staunton (needs unlock_commercial) /
             Shoreside (needs unlock_suburban).
Rule       = a mission is reachable when its contact is unlocked AND its island
             region is reachable.
Victory    = reach Catalina's final mission (the "Victory" event).
"""
from typing import Any, Dict

from BaseClasses import Item, ItemClassification, Location, Region
from worlds.AutoWorld import World, WebWorld
from worlds.LauncherComponents import Component, Type, components, launch_subprocess

from .data import data
from .Options import GTA3Options


def launch_client(*args):
    from .Client import launch
    launch_subprocess(launch, name="GTA3Client", args=args)


components.append(Component(
    "GTA3 Client", func=launch_client, component_type=Type.CLIENT,
    description="Client texte GTA III: /progression, /playable, /missing"))

GAME = "Grand Theft Auto III"

PACKAGE_MILESTONE_REGIONS = {
    "hidden_packages_10": "Portland",
    "hidden_packages_20": "Portland",
    "hidden_packages_30": "Portland",
    "hidden_packages_40": "Staunton",
    "hidden_packages_50": "Staunton",
    "hidden_packages_60": "Staunton",
    "hidden_packages_70": "Shoreside",
    "hidden_packages_80": "Shoreside",
    "hidden_packages_90": "Shoreside",
    "hidden_packages_100": "Shoreside",
}

def iter_world_locations():
    seen = set()
    for loc in data["locations"]:
        seen.add(loc["id"])
        yield loc

    for loc_id, region in PACKAGE_MILESTONE_REGIONS.items():
        if loc_id in seen or loc_id not in data["location_ids"]:
            continue
        yield {"id": loc_id, "chain": None, "region": region}


def build_previous_location_by_chain() -> Dict[str, str]:
    previous_by_location = {}
    last_by_chain = {}
    for loc in data["locations"]:
        chain = loc.get("chain")
        if not chain:
            continue
        loc_id = loc["id"]
        if chain in last_by_chain:
            previous_by_location[loc_id] = last_by_chain[chain]
        last_by_chain[chain] = loc_id
    return previous_by_location


def build_chain_successor_placements() -> Dict[str, str]:
    """ordered_chains: map <previous location id> -> <unlock item of the NEXT
    mission in the same chain>. Those items get locked onto their predecessor
    instead of entering the shuffled pool, so each chain self-advances in
    vanilla order; chain OPENER unlocks stay in the pool."""
    placements = {}
    previous_by_location = build_previous_location_by_chain()
    mission_unlock_by_location = data.get("mission_unlock_by_location", {})
    for loc_id, unlock_item in mission_unlock_by_location.items():
        previous = previous_by_location.get(loc_id)
        if previous and unlock_item:
            placements[previous] = unlock_item
    return placements


def build_mandatory_unlock_placements() -> Dict[str, str]:
    """Unlocks that must be deterministic even when chain ordering is disabled."""
    item_name = "unlock_mission_luigi2"
    if ("mission_give_me_liberty" in data["location_ids"] and
            item_name in data["item_ids"]):
        return {"mission_give_me_liberty": item_name}
    return {}


def build_locked_unlock_placements(ordered_chains: bool) -> Dict[str, str]:
    placements = build_mandatory_unlock_placements()
    if ordered_chains:
        placements.update(build_chain_successor_placements())
    return placements


class GTA3Item(Item):
    game = GAME


class GTA3Location(Location):
    game = GAME


class GTA3Web(WebWorld):
    tutorials = []


class GTA3World(World):
    game = GAME
    web = GTA3Web()
    options_dataclass = GTA3Options
    options: GTA3Options

    item_name_to_id = dict(data["item_ids"])
    location_name_to_id = dict(data["location_ids"])

    def create_item(self, name: str) -> GTA3Item:
        if name in data["progression"]:
            classification = ItemClassification.progression
        elif name in data.get("useful", []):
            classification = ItemClassification.useful
        else:
            classification = ItemClassification.filler
        return GTA3Item(name, classification, self.item_name_to_id[name], self.player)

    def create_items(self) -> None:
        # ordered_chains: the chain-successor unlocks are locked onto their
        # predecessor location in create_regions and must stay OUT of the
        # shuffled pool (and their host locations out of the filler math).
        locked_items = set(build_locked_unlock_placements(
            bool(self.options.ordered_chains)).values())
        pool = [self.create_item(name) for name in data["progression"]
                if name not in locked_items]
        # One of each weapon/armor item (useful tier), replacing cash filler.
        pool += [self.create_item(name) for name in data.get("useful", [])
                 if name in self.item_name_to_id]
        # One item per remaining real (non-event, non-locked) location.
        remaining = len(self.location_name_to_id) - len(locked_items) - len(pool)
        for _ in range(remaining):
            pool.append(self.create_item(data["filler"]))
        self.multiworld.itempool += pool

    def create_regions(self) -> None:
        mw, p = self.multiworld, self.player

        menu = Region("Menu", p, mw)
        mw.regions.append(menu)
        regions: Dict[str, Region] = {}
        for name in ("Portland", "Staunton", "Shoreside"):
            r = Region(name, p, mw)
            mw.regions.append(r)
            regions[name] = r

        menu.connect(regions["Portland"])
        menu.connect(regions["Staunton"],
                     rule=lambda state: state.has("unlock_commercial", p))
        menu.connect(regions["Shoreside"],
                     rule=lambda state: state.has("unlock_suburban", p))

        previous_by_location = build_previous_location_by_chain()
        mission_unlock_by_location = data.get("mission_unlock_by_location", {})

        locations_by_id: Dict[str, GTA3Location] = {}
        for loc in iter_world_locations():
            location = GTA3Location(p, loc["id"], self.location_name_to_id[loc["id"]],
                                    regions[loc["region"]])
            unlock_item = mission_unlock_by_location.get(loc["id"])
            previous_location = previous_by_location.get(loc["id"])
            if unlock_item or previous_location:
                location.access_rule = (
                    lambda state, item=unlock_item, previous=previous_location:
                    (not item or state.has(item, p)) and
                    (not previous or state.can_reach(previous, "Location", p))
                )
            regions[loc["region"]].locations.append(location)
            locations_by_id[loc["id"]] = location

        # Lock mandatory AP boot progression, then ordered chain successors.
        for host_id, item_name in build_locked_unlock_placements(
                bool(self.options.ordered_chains)).items():
            host = locations_by_id.get(host_id)
            if host is not None:
                host.place_locked_item(self.create_item(item_name))

        # Victory: an event gated by Catalina's final mission being logically
        # reachable. The client still sends the real mission_cat1 check in-game.
        victory = GTA3Location(p, "Victory", None, regions["Shoreside"])
        victory.place_locked_item(GTA3Item("Victory", ItemClassification.progression, None, p))
        victory.access_rule = (lambda state:
                               state.can_reach(data["victory_location"], "Location", p))
        regions["Shoreside"].locations.append(victory)

        mw.completion_condition[p] = lambda state: state.has("Victory", p)

    def fill_slot_data(self) -> Dict[str, Any]:
        return {
            "slot_name": self.player_name,
            "seed_name": self.multiworld.seed_name,
            "supports_give_me_liberty_location": True,
            "supports_mission_unlock_items": True,
        }
