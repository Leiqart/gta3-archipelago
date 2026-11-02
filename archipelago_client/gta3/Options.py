from dataclasses import dataclass

from Options import DefaultOnToggle, PerGameCommonOptions


class OrderedChains(DefaultOnToggle):
    """Each mission holds the unlock item of the next mission of its contact, so chains play in vanilla order."""
    display_name = "Ordered mission chains"


@dataclass
class GTA3Options(PerGameCommonOptions):
    ordered_chains: OrderedChains
