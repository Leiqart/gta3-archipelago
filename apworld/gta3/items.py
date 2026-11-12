from collections import Counter
from collections.abc import Sequence
from dataclasses import dataclass

from .missions import CONTACTS, MISSIONS, contact_missions, unlock_item_name


@dataclass(frozen=True)
class ItemData:
    """An item of the apworld: what the room calls it, and what the game gets when our slot receives it."""
    name: str
    id: int
    # How Archipelago treats it, in its own words: "progression" (the logic needs it to reach locations) or "filler".
    classification: str
    # What the mod does with it, in the words of docs/protocol.md: "unlock" a mission by its number, or give "money".
    effect: str
    # What each copy gives, in the order our slot receives them (mission numbers, or an amount): the nth copy gives the nth value, a copy past the last gives the last again.
    values: tuple[int, ...]


# The filler item, given in the game as money.
MONEY = ItemData("$10000", 2, "filler", "money", (10000,))

# Every item of the apworld. Their numbers are stable, like the locations' (see missions.py): never renumber them, never reuse a retired one.
ITEMS = [
    MONEY,
    # Ordered mode: each contact's progressive item, whose copies unlock the contact's missions in story order.
    *(ItemData(contact.progressive_item, contact.progressive_id, "progression", "unlock", tuple(mission.number for mission in contact_missions(contact.name)))
      for contact in CONTACTS),
    # Free mode: each story mission's own unlock; Give Me Liberty has none, as the intro starts it.
    *(ItemData(unlock_item_name(mission), mission.unlock_id, "progression", "unlock", (mission.number,)) for mission in MISSIONS if mission.unlock_id),
]

ITEM_NAMED = {item.name: item for item in ITEMS}
ITEM_OF_ID = {item.id: item for item in ITEMS}


def effects_of(received: Sequence[int], start: int = 0) -> list[tuple[int, str, int]]:
    """What the game gets from the items of our slot's list, given by their ids, from this position to the list's end: each one's position, effect and value, as the mod is told them.
    Position 0 gives the whole list, which a game that loads is sent; a later one gives the items just received. An id that is no item of this apworld gives ("none", 0)."""
    copies = Counter(received[:start])
    effects = []
    for index in range(start, len(received)):
        item = ITEM_OF_ID.get(received[index])
        copies[received[index]] += 1
        if item is None:
            effects.append((index, "none", 0))
        else:
            effects.append((index, item.effect, item.values[min(copies[item.id], len(item.values)) - 1]))
    return effects
