#include <array>
#include <sstream>
#include <string>
#include <type_traits>
#include "plugin.h"
#include "CPickups.h"
#include "ClientLink.h"
#include "Log.h"
#include "Shops.h"

namespace {
    // A shop prices what it sells by its weapon number (eWeaponType, the body armour included) in the game's price list, CostOfWeapon: its size, as plugin-sdk declares it, bounds the numbers.
    constexpr int kWeapons = static_cast<int>(std::extent_v<std::remove_reference_t<decltype(CostOfWeapon)>>);
    // By weapon number: whether the weapon is an item in our slot, and whether our slot has received it.
    std::array<bool, kWeapons> g_isItem{}, g_received{};

    bool IsWeapon(int weapon) { return weapon >= 0 && weapon < kWeapons; }

    // One "weapons <number> ..." line: the weapons that are items in our slot; the list takes the place of the one before, and is logged when it changes.
    void HandleWeapons(const std::string& line) {
        std::istringstream words(line);
        std::string command, list;
        words >> command;
        std::array<bool, kWeapons> isItem{};
        for (int weapon = -1; words >> weapon;) {
            if (!IsWeapon(weapon)) {
                Log::Write("Unknown message from the client: " + line);
                return;
            }
            isItem[weapon] = true;
            list += " " + std::to_string(weapon);
        }
        if (isItem == g_isItem) return;
        g_isItem = isItem;
        Log::Write("Shops: the weapons that are items in this slot:" + (list.empty() ? std::string(" none") : list));
    }

    // Runs once per frame: a shop's pickup of a weapon that is an item is on sale once our slot has received it and out of stock until then; every other pickup is left alone.
    // The game shows an out of stock pickup with its "out of stock" text, and sells nothing from it.
    void Process() {
        for (CPickup& pickup : CPickups::aPickUps) {
            if (pickup.m_nPickupType != PICKUP_IN_SHOP && pickup.m_nPickupType != PICKUP_IN_SHOP_OUT_OF_STOCK) continue;
            // The game's own lookup, which its shops price with: 0 for a model that is no weapon.
            const int weapon = static_cast<int>(CPickups::WeaponForModel(pickup.m_nModelIndex));
            if (IsWeapon(weapon) && g_isItem[weapon]) pickup.m_nPickupType = g_received[weapon] ? PICKUP_IN_SHOP : PICKUP_IN_SHOP_OUT_OF_STOCK;
        }
    }
}

void Shops::Install() {
    ClientLink::On("weapons", HandleWeapons);
    plugin::Events::gameProcessEvent += Process;
}

void Shops::Unlock(int weapon) {
    if (!IsWeapon(weapon)) {
        Log::Write("Shops: weapon " + std::to_string(weapon) + " is not one the game has, nothing to sell");
        return;
    }
    g_received[weapon] = true;
    Log::Write("Shops: weapon " + std::to_string(weapon) + " can be bought");
}
