from collections import Counter
from collections.abc import Sequence
from dataclasses import dataclass

from .missions import CONTACTS, MISSIONS, contact_missions, unlock_item_name


@dataclass(frozen=True)
class ItemData:
    """An item of the apworld: what the room calls it, and what the game gets when our slot receives it."""
    name: str
    id: int
    # How Archipelago treats it, in its own words: "progression" (the logic needs it to reach locations), "useful" (worth having, the logic does not need it) or "filler".
    classification: str
    # What the mod does with it, in the words of docs/protocol.md: "unlock" a mission by its number, give "money", or let the shops sell a "weapon" by its number.
    effect: str
    # What each copy gives, in the order our slot receives them (mission numbers, an amount, or a weapon's number): the nth copy gives the nth value, a copy past the last gives the last again.
    values: tuple[int, ...]
    # True for an item the game uses up (money): a game applies it once, and a later game skips it, as the room keeps what it gave. A lasting one (a mission, a weapon) is applied by every game.
    consumable: bool = False


# The filler item, given in the game as money.
MONEY = ItemData("$10000", 2, "filler", "money", (10000,), consumable=True)

# The weapons the shops sell that are items: a shop shows one out of stock until our slot has received its license.
# Each value is the game's number for the weapon, plugin-sdk's eWeaponType (enums/eWeaponType.h), named at the end of its line.
# The pistol and the body armour are sold as in the game: with them, the licenses would be more than the 9 locations the missions' unlocks leave (see docs/apworld-notes.md).
WEAPONS = [
    ItemData("Uzi License", 80, "useful", "weapon", (3,)),                 # WEAPONTYPE_UZI
    ItemData("Shotgun License", 81, "useful", "weapon", (4,)),             # WEAPONTYPE_SHOTGUN
    ItemData("AK-47 License", 82, "useful", "weapon", (5,)),               # WEAPONTYPE_AK47
    ItemData("M-16 License", 83, "useful", "weapon", (6,)),                # WEAPONTYPE_M16
    ItemData("Sniper Rifle License", 84, "useful", "weapon", (7,)),        # WEAPONTYPE_SNIPERRIFLE
    ItemData("Rocket Launcher License", 85, "useful", "weapon", (8,)),     # WEAPONTYPE_ROCKETLAUNCHER
    ItemData("Flamethrower License", 86, "useful", "weapon", (9,)),        # WEAPONTYPE_FLAMETHROWER
    ItemData("Molotov Cocktail License", 87, "useful", "weapon", (10,)),   # WEAPONTYPE_MOLOTOV
    ItemData("Grenade License", 88, "useful", "weapon", (11,)),            # WEAPONTYPE_GRENADE
]

# Every item of the apworld. Their numbers are stable, like the locations' (see missions.py): never renumber them, never reuse a retired one.
ITEMS = [
    MONEY,
    # Ordered mode: each contact's progressive item, whose copies unlock the contact's missions in story order.
    *(ItemData(contact.progressive_item, contact.progressive_id, "progression", "unlock", tuple(mission.number for mission in contact_missions(contact.name)))
      for contact in CONTACTS),
    # Free mode: each story mission's own unlock; Give Me Liberty has none, as the intro starts it.
    *(ItemData(unlock_item_name(mission), mission.unlock_id, "progression", "unlock", (mission.number,)) for mission in MISSIONS if mission.unlock_id),
    *WEAPONS,
]

ITEM_NAMED = {item.name: item for item in ITEMS}
ITEM_OF_ID = {item.id: item for item in ITEMS}

# The effects of the consumable items, which the client tells the mod: the mod only sees effects, so an effect belongs to consumable items only, or to lasting ones only.
CONSUMABLE_EFFECTS = sorted({item.effect for item in ITEMS if item.consumable})
if set(CONSUMABLE_EFFECTS) & {item.effect for item in ITEMS if not item.consumable}:
    raise ValueError("items.py: an effect belongs both to consumable items and to lasting ones")


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
