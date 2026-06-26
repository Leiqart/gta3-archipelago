#pragma once

namespace Config {
    void Init();
    void Refresh();
    void InstallConfiguredMainScript();

    bool DebugOpenIslands();
    bool KeepWeaponsOnDeath();
    bool UnlockMissionsAtomically();
    bool UseClassicMissionMarkers();
    bool RequireApMissionUnlockItems();
}
