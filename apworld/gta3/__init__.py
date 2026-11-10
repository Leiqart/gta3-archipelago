from BaseClasses import Item, ItemClassification, Location, Region
from worlds.AutoWorld import World
from worlds.LauncherComponents import Component, Type, components, launch_subprocess
from .missions import (ALL_MISSIONS, CONTACTS, GOAL_MISSION, ISLAND_OPENERS, MISSIONS, MONEY_ID, MONEY_ITEM, PORTLAND, SHORESIDE, STAUNTON,
                       Mission, contact_missions, unlock_item_name)
from .options import GTA3Options, MissionOrder


# Runs only when the player clicks the button, in a separate program; the client code is loaded only then.
def launch_client(*args: str) -> None:
    from .client import launch
    launch_subprocess(launch, name="GTA3Client", args=args)


# Adds the "GTA3 Client" button to the Archipelago Launcher.
components.append(Component("GTA3 Client", func=launch_client, component_type=Type.CLIENT))


class GTA3Item(Item):
    game = "Grand Theft Auto III"


class GTA3Location(Location):
    game = "Grand Theft Auto III"


# The event the logic uses for an island: an event location the opening mission makes reachable, holding an event item of the same name.
def island_event(island: str) -> str:
    return f"{island} Open"


class GTA3World(World):
    """Grand Theft Auto III: finish missions to send checks, receive unlocks to play the next ones."""
    game = "Grand Theft Auto III"

    # Where Archipelago finds our options; self.options holds the values read from the player's YAML.
    options_dataclass = GTA3Options
    options: GTA3Options

    # Stable numbers shared by the server, the client and the mod, all read from missions.py.
    location_name_to_id = {mission.name: mission.location_id for mission in ALL_MISSIONS}
    item_name_to_id = {
        MONEY_ITEM: MONEY_ID,
        **{contact.progressive_item: contact.progressive_id for contact in CONTACTS},
        **{unlock_item_name(mission): mission.unlock_id for mission in MISSIONS if mission.unlock_id},
    }

    # Builds one item; the money is the only filler, every unlock is used in a rule so it is progression.
    def create_item(self, name: str) -> GTA3Item:
        classification = ItemClassification.filler if name == MONEY_ITEM else ItemClassification.progression
        return GTA3Item(name, classification, self.item_name_to_id[name], self.player)

    # The map: Menu leads to Portland, which leads to Staunton Island once Last Requests is passed, which leads to Shoreside Vale once A Drop in the Ocean is.
    def create_regions(self) -> None:
        regions = {name: Region(name, self.player, self.multiworld) for name in ("Menu", PORTLAND, STAUNTON, SHORESIDE)}
        for mission in ALL_MISSIONS:
            region = regions[mission.island]
            region.locations.append(GTA3Location(self.player, mission.name, mission.location_id, region))
        # Each island's event sits in the region of the mission that opens it, and is reached with that mission.
        for island, opener in ISLAND_OPENERS.items():
            region = regions[self.mission(opener).island]
            event = GTA3Location(self.player, island_event(island), None, region)
            event.place_locked_item(GTA3Item(island_event(island), ItemClassification.progression, None, self.player))
            event.access_rule = lambda state, opener=opener: state.can_reach(opener, "Location", self.player)
            region.locations.append(event)
        self.multiworld.regions += regions.values()
        regions["Menu"].connect(regions[PORTLAND])
        regions[PORTLAND].connect(regions[STAUNTON], rule=lambda state: state.has(island_event(STAUNTON), self.player))
        regions[STAUNTON].connect(regions[SHORESIDE], rule=lambda state: state.has(island_event(SHORESIDE), self.player))

    # One item per story mission but Give Me Liberty, which the intro starts: its unlock in free mode, its contact's progressive item in ordered mode; money fills the other locations.
    def create_items(self) -> None:
        if self.options.mission_order == MissionOrder.option_free:
            names = [unlock_item_name(mission) for mission in MISSIONS if mission.unlock_id]
        else:
            names = [contact.progressive_item for contact in CONTACTS for _ in contact_missions(contact.name)]
        names += [MONEY_ITEM] * (len(ALL_MISSIONS) - len(names))
        self.multiworld.itempool += [self.create_item(name) for name in names]

    # Each mission gets its rule; passing The Exchange, the end of the story, wins.
    def set_rules(self) -> None:
        for mission in ALL_MISSIONS:
            self.multiworld.get_location(mission.name, self.player).access_rule = self.mission_rule(mission)
        self.multiworld.completion_condition[self.player] = lambda state: state.can_reach(GOAL_MISSION, "Location", self.player)

    # A mission needs its unlock (free) or as many copies of its contact's progressive item as its place in the contact's order (ordered),
    # and the missions whose pass starts its trigger; its island comes from its region.
    def mission_rule(self, mission: Mission):
        requires = mission.requires
        if not mission.unlock_id:
            return lambda state: all(state.can_reach(name, "Location", self.player) for name in requires)
        if self.options.mission_order == MissionOrder.option_free:
            item, count = unlock_item_name(mission), 1
        else:
            contact = next(contact for contact in CONTACTS if contact.name == mission.contact)
            item, count = contact.progressive_item, contact_missions(contact.name).index(mission) + 1
        return lambda state: state.has(item, self.player, count) and all(state.can_reach(name, "Location", self.player) for name in requires)

    # A mission of the table, by its name.
    @staticmethod
    def mission(name: str) -> Mission:
        return next(mission for mission in ALL_MISSIONS if mission.name == name)
