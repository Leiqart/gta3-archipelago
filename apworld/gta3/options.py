from dataclasses import dataclass

from Options import Choice, PerGameCommonOptions


class MissionOrder(Choice):
    """Ordered keeps each boss's story order; free lets any unlocked mission be played (not "random": YAML reserves that word)."""
    display_name = "Mission Order"
    option_ordered = 0
    option_free = 1
    default = 0


@dataclass
class GTA3Options(PerGameCommonOptions):
    mission_order: MissionOrder
