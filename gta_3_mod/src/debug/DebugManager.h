#pragma once
// Central debug orchestration. Hotkey implementations stay in DebugHotkeys.h;
// this manager owns the per-frame polling order so Hooks.cpp does not need to
// know every individual debug action.

namespace PlayerRuntime::DebugManager {
    inline void TickEarlyHotkeys() {
        TryFireTestKey();
        TryFireDeathKey();
    }

    inline void TickMissionHotkeys() {
        TryFireTrapKey();
        TryFireClearWantedKey();
        TryFireUnlockIslandKey();
        TryFireUnlockCharacterKey();
        TryFireUnlockEverythingKey();
        TryFireValidateMissionKey();
        TryFireObjectiveTeleportKey();
    }

    inline void TickWorldHotkeys() {
        TryFireKillNpcsKey();
        TryFireRecordPositionKey();
        TryFireBridgeTeleportKey();
        TryFireSpawnCarKey();
        TryFireGiveWeaponsKey();
        TryFireVehicleSmokeKey();
        TryFireVehicleFireKey();
        TryFireSafehouseTeleportKey();
        TryFirePackageTeleportKey();
        TryFireCyclePlayerSkinKey();
    }

    inline void TickHotkeys() {
        TickEarlyHotkeys();
        TickMissionHotkeys();
        TickWorldHotkeys();
    }
}
