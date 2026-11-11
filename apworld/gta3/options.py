from dataclasses import dataclass

from Options import Choice, DeathLink, DefaultOnToggle, PerGameCommonOptions, Toggle


class MissionOrder(Choice):
    """Ordered keeps each boss's story order; free lets any unlocked mission be played (not "random": YAML reserves that word)."""
    display_name = "Mission Order"
    option_ordered = 0
    option_free = 1
    default = 0


class NeedMoney(Toggle):
    """On, Bomb Da Base: Act II asks for $100000 and The Exchange for $500000 as in the game; off, the mod lets both start without paying."""
    display_name = "Need Money"


class ArrestIsDeath(DefaultOnToggle):
    """With death_link on: being busted also sends a death to the other players, as it ends a mission like being wasted does."""
    display_name = "Arrest Is Death"


@dataclass
class GTA3Options(PerGameCommonOptions):
    mission_order: MissionOrder
    need_money: NeedMoney
    death_link: DeathLink
    arrest_is_death: ArrestIsDeath
