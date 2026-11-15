#include <string>
#include "plugin.h"
#include "CPickups.h"
#include "CPlayerInfo.h"
#include "CPlayerPed.h"
#include "CTimer.h"
#include "CWorld.h"
#include "common.h"
#include "../Log.h"
#include "Actions.h"
#include "Settings.h"
#include "ShopTest.h"

namespace {
    // The Uzi of Portland's Ammu-Nation, where main.scm creates it (CREATE_PICKUP 178 at 1070.5, -400.75, 15.1875): an item in an Archipelago slot.
    const CVector kUzi{1070.5f, -400.75f, 15.1875f};
    // Game milliseconds between two rounds, and how long the player stands on the pickup.
    constexpr int kRoundEvery = 20000;
    constexpr int kOnPickup = 3000;
    // The money the player walks in with: more than the Uzi's price, $800.
    constexpr int kMoney = 10000;

    int g_round = 0;
    int g_nextRound = 0;
    // Game time the player was put on the pickup, 0 between rounds; what he had then, and where he came from.
    int g_onPickupSince = 0;
    int g_ammoBefore = 0;
    CVector g_cameFrom;

    // The shop pickup at this place, on sale or out of stock; nullptr when the game has none there.
    const CPickup* ShopPickupAt(const CVector& place) {
        for (const CPickup& pickup : CPickups::aPickUps) {
            const bool inShop = pickup.m_nPickupType == PICKUP_IN_SHOP || pickup.m_nPickupType == PICKUP_IN_SHOP_OUT_OF_STOCK;
            if (inShop && Actions::Distance2D(pickup.m_vecPos, place) < 1.0f) return &pickup;
        }
        return nullptr;
    }

    int UziAmmo() { return static_cast<int>(FindPlayerPed()->m_aWeapons[WEAPONTYPE_UZI].m_nAmmoTotal); }
}

void ShopTest::Update() {
    if (!Settings::ShopTest() || FindPlayerVehicle()) return;
    const int now = CTimer::m_snTimeInMilliseconds;
    CPlayerInfo& info = CWorld::Players[CWorld::PlayerInFocus];
    const std::string round = "Shop test " + std::to_string(g_round) + ": ";
    if (!g_onPickupSince) {
        if (now < g_nextRound) return;
        ++g_round;
        info.m_nMoney = kMoney;
        g_ammoBefore = UziAmmo();
        g_cameFrom = Actions::PlayerPosition();
        const CPickup* pickup = ShopPickupAt(kUzi);
        const char* state = !pickup ? "missing" : pickup->m_nPickupType == PICKUP_IN_SHOP ? "on sale" : "out of stock";
        Log::Write("Shop test " + std::to_string(g_round) + ": the Uzi pickup is " + state + ", the player walks onto it with $" + std::to_string(kMoney) + " and " + std::to_string(g_ammoBefore) + " Uzi bullets");
        Actions::PlaceAt(kUzi);
        g_onPickupSince = now;
        return;
    }
    if (now - g_onPickupSince < kOnPickup) return;
    const int ammo = UziAmmo();
    Log::Write(round + (ammo > g_ammoBefore ? "the Uzi was bought" : "no Uzi bought") + ", Uzi bullets " + std::to_string(g_ammoBefore) + " -> " + std::to_string(ammo) +
               ", money $" + std::to_string(kMoney) + " -> $" + std::to_string(info.m_nMoney));
    Actions::PlaceAt(g_cameFrom);
    g_onPickupSince = 0;
    g_nextRound = now + kRoundEvery;
}
