#pragma once

namespace RunState {
    void Init();
    void Refresh();

    bool IsRunActive();

    // Session-only live server-connection flag, fed from ApBridge by the
    // per-frame poll. NOT persisted: every game session starts disconnected and
    // only goes true once the bridge authenticates to an Archipelago server.
    void SetServerConnected(bool connected);
    bool ServerConnected();

    // "Run is live" = a run is active (persisted) AND we are genuinely connected
    // to a server this session. The in-world run behaviour (mission unlocks,
    // contact/service blips, the position/money restores, marker launches) gates
    // on THIS, not on IsRunActive(): offline => "offline = pas de run" (plain
    // vanilla free-roam, no tp, no unlock), while the persisted run is preserved
    // for the next connected boot. IsRunActive() stays raw so resume detection
    // never wipes a run just because the bridge is offline.
    bool IsRunLive();

    bool IsRunStartPending();
    bool ShouldSkipIntroOnNextNewGame();
    int Checks();

    // AP map progression state, driven from the .ini and consumed by the
    // SCM mission_start polling loop through the `unused_2` global:
    //   0 = Portland only (boot default)
    //   1 = Portland + Staunton unlocked
    //   2 = all islands unlocked
    int MapState();
    void SetMapState(int state);

    int UnlockedMissionCount(int missionCount);
    bool IsMissionUnlocked(int missionIndex, int missionCount);

    // Number of unlocked mission characters (contacts), MissionBucket order from
    // Luigi. 1 = only Luigi. F2 raises it to reveal the next character.
    int UnlockedCharacters();
    void SetUnlockedCharacters(int count);
    bool IsBucketUnlocked(int bucketOrdinal);
    void UnlockBucket(int bucketOrdinal);

    // Session-local validated missions, keyed by actual main.scm mission index.
    // Durable AP progression is server-owned checked locations; these latches
    // only debounce the current runtime until bridge/server state catches up.
    bool IsMissionValidated(int actualMissionIndex);
    void ValidateMission(int actualMissionIndex);
    bool IsSyntheticMissionValidated(const char* key);
    void ValidateSyntheticMission(const char* key);
    int  ValidatedMissionCount();

    // Persisted player money total, mirrored from the live in-game cash
    // (CWorld_PlayerCash) so it survives disconnect/reconnect. Tracked
    // independently of mission validation: on resume the live cash is
    // overwritten with this value, then kept in sync as the player earns/spends.
    int  Money();
    void SetMoney(int amount);

    // Persisted player loadout/vitals/position, mirrored from the live ped the
    // same way money is (restore once per session, then periodic sync) so a
    // run resumes with the weapons, health, armor and position it was left at.
    constexpr int kPersistWeaponSlots = 13;
    // Fills types/ammo (kPersistWeaponSlots entries each); returns true if at
    // least one armed slot was persisted.
    bool SavedWeapons(int* types, int* ammo);
    void SetSavedWeapons(const int* types, const int* ammo);
    // Health <= 0 means "nothing persisted yet" (never restore a dead state).
    int  SavedHealth();
    int  SavedArmor();
    void SetSavedVitals(int health, int armor);
    bool SavedPosition(float* x, float* y, float* z);
    void SetSavedPosition(float x, float y, float z);

    // Hidden-package progression: the engine's collected count plus the indices
    // (into PackagePoints.h order) identified as collected, so a resumed run
    // does not restart at 0/100.
    int  CollectedPackages();
    void SetCollectedPackages(int count);
    bool IsPackageCollected(int pointIndex);
    void MarkPackageCollected(int pointIndex);

    // Actual index of the most recently launched mission (runtime only, not
    // persisted). -1 when none launched this session. Used to know which
    // mission to validate on completion / via the debug key.
    int  LastLaunchedMission();
    void SetLastLaunchedMission(int actualMissionIndex);
    int  PendingValidationMission();
    const char* PendingValidationSyntheticMission();
    void SetPendingValidationMission(int actualMissionIndex, const char* syntheticKey = nullptr);
    void ClearPendingValidationMission();

    // True when the persisted run carries player-state worth resuming: money,
    // weapons, hidden packages or a saved position. AP mission progression is
    // server-owned checked locations and must not be restored from local disk.
    bool HasAnyRunProgress();

    // Persisted one-shot request: the player asked for the intro cinematic
    // (APINTRO menu entry); the next "Run Archipelago" boots through the
    // vanilla-style intro launch even for a resumed run, then clears this.
    bool IsBootIntroRequested();
    void RequestBootIntro();
    void ClearBootIntroRequest();

    void ActivateRun();
    void BeginNewRun();
    void EndRun();
    void SetChecks(int checks);
    void SetRunStartPending(bool pending);
    void SetSkipIntroOnNextNewGame(bool skip);
}
