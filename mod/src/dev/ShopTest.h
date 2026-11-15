#pragma once

// Dev builds only (shop_test=1): checks in the game that a shop sells a weapon only once its license is received.
// Every 20 s of free play, the player walks onto the Uzi of Portland's Ammu-Nation with $10000, and the log says whether the game sold it.
namespace ShopTest {
    // Runs one step of the check; call it once per frame on the game thread, while the player exists and no cutscene or script holds him (Cheats does).
    void Update();
}
