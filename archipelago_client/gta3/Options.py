from dataclasses import dataclass

from Options import DefaultOnToggle, PerGameCommonOptions


class OrderedChains(DefaultOnToggle):
    """Each mission location hands out the unlock item for the NEXT mission of
    the same contact chain (mission_luigi2 contains unlock_mission_luigi3,
    ...). Solo seeds then play out mostly in vanilla order: only the chain
    openers, the island unlocks, the weapons and the cash stay shuffled across
    packages, RC missions and the chain locations themselves. Disable for a
    fully shuffled multiworld experience."""
    display_name = "Ordered mission chains"


@dataclass
class GTA3Options(PerGameCommonOptions):
    ordered_chains: OrderedChains
