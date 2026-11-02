"""Grand Theft Auto III world: missions and package milestones are checks, per-mission unlocks are items."""
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
    description="Client GTA III + pont ASI local sur 127.0.0.1:2727"))

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


def build_story_links_by_target() -> Dict[str, list]:
    links_by_target = {}
    for link in data.get("mission_story_links", []):
        links_by_target.setdefault(link["target"], []).append(link)
    return links_by_target


def build_mission_access_requirements(
        location_id: str,
        previous_by_location=None,
        links_by_target=None) -> Dict[str, Any]:
    """Return the unlock item, the story openers (OR) and the prerequisites (AND) of one mission."""
    if previous_by_location is None:
        previous_by_location = build_previous_location_by_chain()
    if links_by_target is None:
        links_by_target = build_story_links_by_target()

    links = links_by_target.get(location_id, [])
    opener_triggers = [
        link["trigger"] for link in links if link["kind"] == "opens_chain"]
    replacement_triggers = [
        link["trigger"] for link in links
        if link["kind"] == "replaces_previous"]
    additional_triggers = [
        link["trigger"] for link in links
        if link["kind"] == "adds_prerequisite"]

    if replacement_triggers:
        prerequisites = replacement_triggers
    else:
        previous = previous_by_location.get(location_id)
        prerequisites = [previous] if previous else []
    prerequisites.extend(additional_triggers)

    # Preserve declaration order while making accidental duplicate edges inert.
    prerequisites = list(dict.fromkeys(prerequisites))
    return {
        "unlock_item": data.get("mission_unlock_by_location", {}).get(location_id),
        "opener_triggers": opener_triggers,
        "prerequisites": prerequisites,
    }


def make_mission_access_rule(player: int, requirements: Dict[str, Any]):
    unlock_item = requirements["unlock_item"]
    opener_triggers = tuple(requirements["opener_triggers"])
    prerequisites = tuple(requirements["prerequisites"])

    def rule(state):
        if opener_triggers:
            # Meat Factory has no unlock item, so its opener is story-only.
            gate_open = (
                (bool(unlock_item) and state.has(unlock_item, player)) or
                any(state.can_reach(trigger, "Location", player)
                    for trigger in opener_triggers)
            )
        else:
            gate_open = not unlock_item or state.has(unlock_item, player)
        return gate_open and all(
            state.can_reach(parent, "Location", player)
            for parent in prerequisites)

    return rule


def make_region_access_rule(player: int, item_name: str, story_location: str):
    return lambda state: (
        state.has(item_name, player) or
        state.can_reach(story_location, "Location", player)
    )


def build_chain_successor_placements() -> Dict[str, str]:
    """Map each chain mission to the unlock item of the next one, so ordered chains advance by themselves."""
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
    return {
        "mission_give_me_liberty": "unlock_mission_8ball",
        "mission_8ball": "unlock_mission_luigi2",
    }


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

    def get_filler_item_name(self) -> str:
        return "cash"

    def create_item(self, name: str) -> GTA3Item:
        if name in data["progression"]:
            classification = ItemClassification.progression
        elif name in data.get("useful", []):
            classification = ItemClassification.useful
        else:
            classification = ItemClassification.filler
        return GTA3Item(name, classification, self.item_name_to_id[name], self.player)

    def create_items(self) -> None:
        # Items locked in create_regions stay out of the pool and out of the filler count.
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
        staunton_entrance = menu.connect(regions["Staunton"],
                     rule=make_region_access_rule(
                         p, "unlock_commercial",
                         data["region_story_requirements"]["Staunton"]))
        shoreside_entrance = menu.connect(regions["Shoreside"],
                     rule=make_region_access_rule(
                         p, "unlock_suburban",
                         data["region_story_requirements"]["Shoreside"]))

        previous_by_location = build_previous_location_by_chain()
        links_by_target = build_story_links_by_target()

        locations_by_id: Dict[str, GTA3Location] = {}
        for loc in iter_world_locations():
            location = GTA3Location(p, loc["id"], self.location_name_to_id[loc["id"]],
                                    regions[loc["region"]])
            requirements = build_mission_access_requirements(
                loc["id"], previous_by_location, links_by_target)
            if (requirements["unlock_item"] or
                    requirements["opener_triggers"] or
                    requirements["prerequisites"]):
                location.access_rule = make_mission_access_rule(p, requirements)
            regions[loc["region"]].locations.append(location)
            locations_by_id[loc["id"]] = location

        # Lock mandatory AP boot progression, then ordered chain successors.
        for host_id, item_name in build_locked_unlock_placements(
                bool(self.options.ordered_chains)).items():
            host = locations_by_id.get(host_id)
            if host is not None:
                host.place_locked_item(self.create_item(item_name))

        # Victory event: reachable with mission_cat1, whose real check is still sent by the game.
        victory = GTA3Location(p, "Victory", None, regions["Shoreside"])
        victory.place_locked_item(GTA3Item("Victory", ItemClassification.progression, None, p))
        victory.access_rule = (lambda state:
                               state.can_reach(data["victory_location"], "Location", p))
        regions["Shoreside"].locations.append(victory)

        mw.completion_condition[p] = lambda state: state.has("Victory", p)

        # These entrances inspect story locations, whose rules can themselves
        # depend on other regions. Re-evaluate as each dependency becomes reachable.
        for entrance in (staunton_entrance, shoreside_entrance):
            for region in regions.values():
                mw.register_indirect_condition(region, entrance)

    def fill_slot_data(self) -> Dict[str, Any]:
        return {
            "slot_name": self.player_name,
            "seed_name": self.multiworld.seed_name,
            "supports_give_me_liberty_location": True,
            "supports_mission_unlock_items": True,
            "ordered_chains": bool(self.options.ordered_chains),
        }
