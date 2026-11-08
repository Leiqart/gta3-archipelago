from BaseClasses import Item, ItemClassification, Location, Region
from worlds.AutoWorld import World
from worlds.LauncherComponents import Component, Type, components, launch_subprocess
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


class GTA3World(World):
    """Grand Theft Auto III: finish missions to send checks, receive unlocks to play the next ones."""
    game = "Grand Theft Auto III"

    # Where Archipelago finds our options; self.options holds the values read from the player's YAML.
    options_dataclass = GTA3Options
    options: GTA3Options

    # Stable numbers shared by the server, the client and the mod; never renumber them, and never reuse a retired one.
    # Give Me Liberty covers Luigi's Girls too: the game plays both in one script (EIGHT) with a single Mission passed.
    # Retired ids: location 2 (Luigi's Girls) and item 3 (Luigi's Girls Unlock), merged into Give Me Liberty.
    location_name_to_id = {
        "Give Me Liberty": 1,
        "Don't Spank Ma Bitch Up": 3,
        "Drive Misty For Me": 4,
    }
    item_name_to_id = {
        "Progressive Luigi Mission": 1,
        "$10000": 2,
        "Don't Spank Ma Bitch Up Unlock": 4,
        "Drive Misty For Me Unlock": 5,
    }

    # Builds one item; the money is the only filler, every unlock is used in a rule so it is progression.
    def create_item(self, name: str) -> GTA3Item:
        if name == "$10000":
            classification = ItemClassification.filler
        else:
            classification = ItemClassification.progression
        return GTA3Item(name, classification, self.item_name_to_id[name], self.player)

    # The map: Menu leads to Portland, which holds the three missions.
    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        portland = Region("Portland", self.player, self.multiworld)
        portland.add_locations(self.location_name_to_id, GTA3Location)
        self.multiworld.regions += [menu, portland]
        menu.connect(portland)

    # Three locations, so three items: two Luigi unlocks chosen by the mode, and the money.
    def create_items(self) -> None:
        if self.options.mission_order == MissionOrder.option_free:
            unlocks = ["Don't Spank Ma Bitch Up Unlock", "Drive Misty For Me Unlock"]
        else:
            unlocks = ["Progressive Luigi Mission", "Progressive Luigi Mission"]
        self.multiworld.itempool += [self.create_item(name) for name in unlocks + ["$10000"]]

    # Free: each mission needs its own unlock. Ordered: each Luigi mission needs one more progressive copy.
    def set_rules(self) -> None:
        dont_spank = self.multiworld.get_location("Don't Spank Ma Bitch Up", self.player)
        drive_misty = self.multiworld.get_location("Drive Misty For Me", self.player)
        if self.options.mission_order == MissionOrder.option_free:
            dont_spank.access_rule = lambda state: state.has("Don't Spank Ma Bitch Up Unlock", self.player)
            drive_misty.access_rule = lambda state: state.has("Drive Misty For Me Unlock", self.player)
        else:
            dont_spank.access_rule = lambda state: state.has("Progressive Luigi Mission", self.player, 1)
            drive_misty.access_rule = lambda state: state.has("Progressive Luigi Mission", self.player, 2)
        self.multiworld.completion_condition[self.player] = (
            lambda state: state.can_reach("Drive Misty For Me", "Location", self.player))
