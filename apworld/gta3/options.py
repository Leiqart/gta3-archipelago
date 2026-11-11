from dataclasses import dataclass

from Options import Choice, PerGameCommonOptions, Toggle


class MissionOrder(Choice):
    """Ordered keeps each boss's story order; free lets any unlocked mission be played (not "random": YAML reserves that word)."""
    display_name = "Mission Order"
    option_ordered = 0
    option_free = 1
    default = 0


class NeedMoney(Toggle):
    """On, Bomb Da Base: Act II asks for $100000 and The Exchange for $500000 as in the game; off, the mod lets both start without paying."""
    display_name = "Need Money"


@dataclass
class GTA3Options(PerGameCommonOptions):
    mission_order: MissionOrder
    need_money: NeedMoney
