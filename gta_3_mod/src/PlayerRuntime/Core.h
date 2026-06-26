#pragma once
// PlayerRuntime: Core (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    struct CVector {
        float x;
        float y;
        float z;
    };

    struct SpawnPoint {
        CVector position;
        float   heading;
    };

    constexpr SpawnPoint kIndustrialSafehouse = {{883.5f, -308.2f, 7.6f}, 90.0f};
    constexpr SpawnPoint kCommercialSafehouse = {{103.0f, -478.5f, 14.9f}, 0.0f};
    constexpr SpawnPoint kSuburbanSafehouse   = {{-666.8f, -1.8f, 19.0f}, 180.0f};
    // INTRO launches from runtime instead of a vanilla boot path, so we keep
    // explicit scene anchors for both the bank cutscene and the post-cutscene
    // bridge handoff.
    constexpr CVector    kIntroBankScene      = {1490.153f, 1310.493f, 129.797f};
    constexpr CVector    kGiveMeLibertyScene  = {811.9f, -939.95f, 35.8f};
    constexpr int        kWarpStabilizeFrames = 60;
    constexpr int        kFastWarpStabilizeFrames = 10;

    // Per-mission spawn points. Coordinates were extracted from the
    // *_mission_loop LOCATE_PLAYER_ON_FOOT_* checks in main.sc: those are the
    // trigger markers the engine normally waits Claude to stand on before
    // LOAD_AND_LAUNCH_MISSION fires. Launching from the pause menu skips that
    // wait, so the mission script either opens on an empty scene (EIGHT,
    // whose intro cutscene fixes the camera on the bank fire while Claude is
    // still at the safehouse) or relocates Claude via its own
    // SET_PLAYER_COORDINATES later (LM2+ all do this from the cutscene
    // staging area). Pre-teleporting Claude to the trigger pose lets both
    // styles land cleanly.
    struct MissionSpawn {
        int actualIndex;
        SpawnPoint spawn;
        ScriptRuntime::MissionLaunchVariant variant = ScriptRuntime::MissionLaunchVariant::Standard;
    };
    constexpr MissionSpawn kMissionSpawns[] = {
        {2,  {{811.9f, -939.95f, 35.8f}, 180.0f}, ScriptRuntime::MissionLaunchVariant::IntroToLuigisGirls},
        // MEAT1..MEAT4 -- meat factory cutscene/start marker.
        {17, {{1223.9f, -839.2f, 13.9f}, 0.0f}},
        {18, {{1223.9f, -839.2f, 13.9f}, 0.0f}},
        {19, {{1223.9f, -839.2f, 13.9f}, 0.0f}},
        {20, {{1223.9f, -839.2f, 13.9f}, 0.0f}},
        // EIGHT / "Give Me Liberty" — bank fire scene, near 8-Ball.
        {21, {{811.9f, -939.95f, 35.8f}, 180.0f}},
        {21, {{892.8f, -425.8f, 13.9f},   0.0f}, ScriptRuntime::MissionLaunchVariant::LuigisGirlsDirect},
        // LUIGI2..LUIGI5 — Luigi's club front door (Sex Club Seven).
        {22, {{892.8f, -425.8f, 13.9f},   0.0f}},
        {23, {{892.8f, -425.8f, 13.9f},   0.0f}},
        {24, {{892.8f, -425.8f, 13.9f},   0.0f}},
        {25, {{892.8f, -425.8f, 13.9f},   0.0f}},
        // JOEY1..JOEY6 — Joey's garage.
        {26, {{1191.7f, -870.0f, 15.0f}, 0.0f}},
        {27, {{1191.7f, -870.0f, 15.0f}, 0.0f}},
        {28, {{1191.7f, -870.0f, 15.0f}, 0.0f}},
        {29, {{1191.7f, -870.0f, 15.0f}, 0.0f}},
        {30, {{1191.7f, -870.0f, 15.0f}, 0.0f}},
        {31, {{1191.7f, -870.0f, 15.0f}, 0.0f}},
        // TONI1..TONI5 — Momma's restaurant.
        {32, {{1219.8f, -319.7f, 27.4f}, 0.0f}},
        {33, {{1219.8f, -319.7f, 27.4f}, 0.0f}},
        {34, {{1219.8f, -319.7f, 27.4f}, 0.0f}},
        {35, {{1219.8f, -319.7f, 27.4f}, 0.0f}},
        {36, {{1219.8f, -319.7f, 27.4f}, 0.0f}},
        // FRANK1..FRANK4 — Salvatore's mansion.
        {37, {{1455.7f, -187.3f, 55.6f}, 0.0f}},
        {38, {{1455.7f, -187.3f, 55.6f}, 0.0f}},
        {39, {{1455.7f, -187.3f, 55.6f}, 0.0f}},
        {40, {{1455.7f, -187.3f, 55.6f}, 0.0f}},
        {41, {{1455.7f, -187.3f, 55.6f}, 0.0f}},
        // DIABLO1..DIABLO4 — Diablos phone booth.
        {42, {{938.4f, -230.5f, 15.0f}, 0.0f}},
        {43, {{938.4f, -230.5f, 15.0f}, 0.0f}},
        {44, {{938.4f, -230.5f, 15.0f}, 0.0f}},
        {45, {{938.4f, -230.5f, 15.0f}, 0.0f}},
        // ASUKA1..ASUKA5 -- Asuka's condo.
        {46, {{523.6f, -639.4f, 16.6f}, 0.0f}},
        {47, {{523.6f, -639.4f, 16.6f}, 0.0f}},
        {48, {{523.6f, -639.4f, 16.6f}, 0.0f}},
        {49, {{523.6f, -639.4f, 16.6f}, 0.0f}},
        {50, {{523.6f, -639.4f, 16.6f}, 0.0f}},
        // KENJI1..KENJI5 -- Kenji's casino.
        {51, {{459.1f, -1413.0f, 26.1f}, 0.0f}},
        {52, {{459.1f, -1413.0f, 26.1f}, 0.0f}},
        {53, {{459.1f, -1413.0f, 26.1f}, 0.0f}},
        {54, {{459.1f, -1413.0f, 26.1f}, 0.0f}},
        {55, {{459.1f, -1413.0f, 26.1f}, 0.0f}},
        // RAY1..RAY6 -- Belleville Park toilets.
        {56, {{38.8f, -725.5f, 22.8f}, 0.0f}},
        {57, {{38.8f, -725.5f, 22.8f}, 0.0f}},
        {58, {{38.8f, -725.5f, 22.8f}, 0.0f}},
        {59, {{38.8f, -725.5f, 22.8f}, 0.0f}},
        {60, {{38.8f, -725.5f, 22.8f}, 0.0f}},
        {61, {{38.8f, -725.5f, 22.8f}, 0.0f}},
        // LOVE1..LOVE7 -- Donald Love's tower.
        {62, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        {63, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        {64, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        // YARD1..YARD4 -- King Courtney's payphone.
        {65, {{120.7f, -272.1f, 16.1f}, 0.0f}},
        {66, {{120.7f, -272.1f, 16.1f}, 0.0f}},
        {67, {{120.7f, -272.1f, 16.1f}, 0.0f}},
        {68, {{120.7f, -272.1f, 16.1f}, 0.0f}},
        {69, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        {70, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        {71, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        {72, {{86.1f, -1548.7f, 28.2f}, 0.0f}},
        // ASUSB1..ASUSB3 and CAT1 -- Shoreside mansion.
        {73, {{-363.7f, 246.1f, 60.0f}, 0.0f}},
        {74, {{-363.7f, 246.1f, 60.0f}, 0.0f}},
        {75, {{-363.7f, 246.1f, 60.0f}, 0.0f}},
        // HOOD1..HOOD5 -- D-Ice's payphone.
        {76, {{-443.5f, -6.1f, 3.8f}, 0.0f}},
        {77, {{-443.5f, -6.1f, 3.8f}, 0.0f}},
        {78, {{-443.5f, -6.1f, 3.8f}, 0.0f}},
        {79, {{-443.5f, -6.1f, 3.8f}, 0.0f}},
        {80, {{-443.5f, -6.1f, 3.8f}, 0.0f}},
        {81, {{-362.8f, 245.9f, 60.0f}, 0.0f}},
    };

    struct StoryMissionUnlockDef {
        int triggerActualIndex;
        ScriptRuntime::MissionBucket bucket;
        const char* unlockDisplayKey;
    };

    constexpr StoryMissionUnlockDef kStoryMissionUnlocks[] = {
        {23, ScriptRuntime::MissionBucket::Joey, "JM1"},
        {23, ScriptRuntime::MissionBucket::MeatFactory, "MEA1"},
        {29, ScriptRuntime::MissionBucket::Toni, "TM1"},
        {34, ScriptRuntime::MissionBucket::Frankie, "FM1"},
        {41, ScriptRuntime::MissionBucket::Asuka, "AM1"},
    };

    inline const MissionSpawn* FindMissionSpawn(
        int actualIndex,
        ScriptRuntime::MissionLaunchVariant variant) {
        const MissionSpawn* fallback = nullptr;
        for (const MissionSpawn& entry : kMissionSpawns) {
            if (entry.actualIndex == actualIndex) {
                if (entry.variant == variant) {
                    return &entry;
                }
                if (!fallback && entry.variant == ScriptRuntime::MissionLaunchVariant::Standard) {
                    fallback = &entry;
                }
            }
        }
        return fallback;
    }

    inline SpawnPoint MissionVehicleParking(const MissionSpawn& mission) {
        const float radians = mission.spawn.heading * 3.1415926535f / 180.0f;
        const float forwardX = std::sin(radians);
        const float forwardY = std::cos(radians);
        const float rightX = forwardY;
        const float rightY = -forwardX;
        SpawnPoint parking = mission.spawn;
        parking.position.x += rightX * 4.0f - forwardX * 10.0f;
        parking.position.y += rightY * 4.0f - forwardY * 10.0f;
        return parking;
    }

    inline void LaunchAddScoreStub(int amount);

    inline bool g_pendingSafehouseWarp = false;
    inline int  g_safehouseWarpFrames  = 0;
    inline bool g_fastSafehouseWarp    = false;
    inline bool g_safehouseWarpOutfitApplied = false;
    inline bool g_gmlToLm1CleanupApplied = false;
    inline bool g_runStartOutfitPreloadApplied = false;
    inline bool g_pendingStartupApStateReset = false;
    // Claude is stuck in the brown prison outfit whenever the intro/GML
    // sequence ends without reaching its redress block (death, quit, or the
    // AP split stopping EIGHT at the hideout). Queue a restore-to-PLAYERX
    // that fires once the ped is alive and settled.
    inline bool g_outfitRestorePending = false;
    inline void QueuePlayerOutfitRestore(const char* reason) {
        g_outfitRestorePending = true;
        Logger::Log("Player outfit restore queued (%s)",
                    reason ? reason : "unspecified");
    }

    // Classic-marker mode: a vanilla street marker refused the launch (the
    // mission's AP unlock item has not arrived). Queued from the
    // StartNewScript hook, fired from the per-frame tick so helper scripts
    // are never started re-entrantly inside the hook.
    inline bool g_markerBlockedToastPending = false;
    inline void QueueMarkerBlockedToast() {
        g_markerBlockedToastPending = true;
    }
    inline bool g_offlineRunToastPending = false;
    inline void QueueOfflineRunToast() {
        g_offlineRunToastPending = true;
    }

    // Resume reveal: after the resume warp lands, the screen stays BLACK until
    // the run data is actually live — the Archipelago data-storage decision
    // arrived (or timed out) and the money/position/loadout restores were
    // applied. Kills the visible "spawn, then teleport, then money pops"
    // sequence. Armed by the safehouse warp, cleared by TryApplyResumeReveal.
    inline bool g_resumeRevealPending = false;
    inline int  g_resumeRevealFrames  = 0;
    constexpr int kResumeRevealTimeoutFrames = 12 * 60; // ~12 s safety valve
    inline bool g_storageSettledThisBoot = false;

    // Offline resume guard: when a run is RESUMED (run already active on boot) but
    // the Archipelago bridge is NOT connected, no run data can ever download.
    // Instead of dropping the player into the world at the safehouse with zero
    // data (the "silent tp into an empty zone"), hold the screen black:
    //   - bridge connects within the window -> reveal the run normally;
    //   - stays offline -> show "No Connected", then reveal the world for plain
    //     free-roam exploration.
    // ASI-only; no SCM change. Tunable timing below (the log prints the real
    // bridge-connect time so the grace can be tuned to it).
    enum class OfflineResumeGuard { Idle, Holding, Done };
    inline OfflineResumeGuard g_offlineResumeGuard = OfflineResumeGuard::Idle;
    inline int  g_offlineResumeGuardFrames = 0;
    inline bool g_offlineResumeGuardToastShown = false;
    constexpr int kOfflineGuardGraceFrames  = 3 * 60;   // ~3 s silent black: window for the bridge to connect
    constexpr int kOfflineGuardRevealFrames = 13 * 60;  // ~13 s total -> reveal world for free-roam
    constexpr int kOfflineGuardMessageMs    = 10000;    // "No Connected" shown ~10 s

    // Money persistence: once per spawned session we overwrite the live cash with
    // the persisted RunState total (restore), then mirror the live cash back into
    // RunState so it survives disconnect/reconnect. Disk writes are throttled to
    // a periodic tick — cash changes every frame during a pickup/spend burst and
    // the .ini does not need to follow each dollar.
    inline bool g_moneyRestoredThisSession = false;
    inline int  g_lastSyncedMoney          = -1;
    inline int  g_moneySyncFrames          = 0;
    constexpr int kMoneySyncIntervalFrames = 30;

    // INTRO stores its cutscene-skip state in global &4456 in the active AP
    // main.scm (main_active_after_lm1fix.ir2:10455). Older decompiles had this
    // at &6112, but in the active script &6112 is not the INTRO skip flag, so
    // writing there lets the bank/Liberty Tree sequence run until it crashes.
    constexpr std::ptrdiff_t kIntroSkipFlagOffset = 4456;
    constexpr int kIntroSkipPumpFrames = 60 * 30; // 30 s at 60 fps
    inline int g_introSkipPumpFrames = 0;

    using TeleportFn = void(__thiscall*)(void* ped, CVector pos);

    inline void* PlayerPed() {
        return *reinterpret_cast<void**>(GameAddr::Translate(GameAddr::FindPlayerPed_Pointer));
    }

    inline void* PlayerVehicle() {
        void* ped = PlayerPed();
        if (!ped) {
            return nullptr;
        }
        return *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_pMyVehicle);
    }

    // Islands open as soon as a contact that LIVES on them is unlocked — and a
    // contact unlocks via its story-mission trigger (kStoryMissionUnlocks) OR
    // its Archipelago unlock item. So "an island opens from the mission that
    // unlocks it OR from a check" falls out for free, with no manual trigger.
    //   Staunton  (1): Asuka, Kenji, Ray, DonaldLove, Yardie
    //   Shoreside (2): Hood, Catalina, AsukaSuburban
    inline int ContactUnlockMapState() {
        using B = ScriptRuntime::MissionBucket;
        for (B b : {B::Hood, B::Catalina, B::AsukaSuburban}) {
            if (ScriptRuntime::IsCharacterUnlocked(b)) {
                return 2;
            }
        }
        for (B b : {B::Asuka, B::Kenji, B::Ray, B::DonaldLove, B::Yardie}) {
            if (ScriptRuntime::IsCharacterUnlocked(b)) {
                return 1;
            }
        }
        return 0;
    }

    inline int EffectiveMapState() {
        // Highest of: F4 debug override (MapState), AP "unlock region" items,
        // and the island any unlocked contact lives on (mission/check driven).
        return (std::max)((std::max)(RunState::MapState(),
                                     ScriptRuntime::ApRegionMapState()),
                          ContactUnlockMapState());
    }

    inline const SpawnPoint& CurrentSafehouse() {
        const int state = EffectiveMapState();
        if (state >= 2) {
            return kSuburbanSafehouse;
        }
        if (state >= 1) {
            return kCommercialSafehouse;
        }
        return kIndustrialSafehouse;
    }

    // CPed inherits CPlaceable:
    //   CPlaceable: vtable @ 0x00, CMatrix m_matrix @ 0x04
    //   CMatrix: right @ 0x00, up @ 0x10, at @ 0x20, pos @ 0x30
    // So Claude's world position lives at 0x04 + 0x30 = 0x34. We previously
    // had 0x44 (m_bOwnsAttachedMatrix), which read garbage as floats and
    // produced LoadScene calls at (0,0,0).
    constexpr std::ptrdiff_t kOff_PedPosition = 0x34;
    // Fire immediately so the cutscene already has streaming requests in
    // flight, then keep pumping every few frames for several seconds to cover
    // skip → loading → actual gameplay start.
    constexpr int            kStreamFlushDelayFrames    = 0;
    constexpr int            kStreamFlushDurationFrames = 300;
    constexpr int            kStreamFlushStrideFrames   = 10;

    inline int  g_streamFlushCountdown = 0;
    inline int  g_streamFlushApplied   = 0;

    // INTRO launches from freeroam with Claude still near the safehouse. Drain
    // the script's first streaming batch once, then leave the normal streamer
    // alone; repeated synchronous drains caused visible hitches during GML.
    inline int  g_introBankSceneFramesLeft = 0;
    constexpr int kIntroBankSceneWatchFrames = 31;
    constexpr int kIntroBankSceneWatchStride = 30;
    inline int  g_manualIntroTriggerFrames = 0;
    constexpr int kManualIntroTriggerDelayFrames = 3;
    constexpr std::uint32_t kLuigiHelpMessageGlobalOffset = 36;
    constexpr std::int32_t  kLaunchIntroFromPauseValue = 99;

    // INTRO mission teleport watcher: after we launch INTRO from runtime the
    // script will SET_PLAYER_COORDINATES Claude onto the Callahan Bridge once
    // the jailbreak cutscene ends. Watch for that handoff, but do not force a
    // second LoadScene there: that synchronous streamer reset is the last
    // custom operation before the current freeze.
    inline bool g_introTeleportArmed   = false;
    inline int  g_introTeleportFramesLeft = 0;
    constexpr int kIntroTeleportWatchFrames = 60 * 60 * 8; // 8 min @60fps
    constexpr float kIntroTeleportRadiusSq  = 50.0f * 50.0f;
    inline int  g_contactBlipCleanupFramesLeft = 0;
    inline int  g_contactBlipCleanupApplied = 0;
    constexpr int kContactBlipCleanupFrames = 180;
    constexpr int kContactBlipCleanupStride = 10;

    using LoadSceneFn = void(__cdecl*)(const CVector*);
    using LoadAllRequestedModelsFn = void(__cdecl*)(bool onlyPriorityRequests);

    inline bool TryLoadSceneAt(const CVector& pos, const char* reason) {
        auto loadScene = reinterpret_cast<LoadSceneFn>(
            GameAddr::Translate(GameAddr::CStreaming_LoadScene));
        __try {
            loadScene(&pos);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("%s: LoadScene raised exception 0x%08lX at (%.1f,%.1f,%.1f)",
                        reason, GetExceptionCode(), pos.x, pos.y, pos.z);
            return false;
        }
        Logger::Log("%s: LoadScene at (%.1f,%.1f,%.1f)",
                    reason, pos.x, pos.y, pos.z);
        return true;
    }

    inline bool TryLoadAllRequestedModels(const char* reason,
                                          bool onlyPriorityRequests = false) {
        auto loadAllRequested = reinterpret_cast<LoadAllRequestedModelsFn>(
            GameAddr::Translate(GameAddr::CStreaming_LoadAllRequestedModels));
        __try {
            loadAllRequested(onlyPriorityRequests);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("%s: LoadAllRequestedModels raised exception 0x%08lX",
                        reason, GetExceptionCode());
            return false;
        }
        Logger::Log("%s: LoadAllRequestedModels(priorityOnly=%d)",
                    reason, onlyPriorityRequests ? 1 : 0);
        return true;
    }

    inline bool TryReadPlaceablePos(void* entity, CVector* out) {
        if (!entity) {
            return false;
        }
        const float* pos = reinterpret_cast<const float*>(
            static_cast<std::uint8_t*>(entity) + kOff_PedPosition);
        out->x = pos[0];
        out->y = pos[1];
        out->z = pos[2];
        return true;
    }

    inline bool TryReadPlayerPos(CVector* out) {
        return TryReadPlaceablePos(PlayerPed(), out);
    }

    inline bool TryReadVehiclePos(CVector* out) {
        return TryReadPlaceablePos(PlayerVehicle(), out);
    }

    inline int CountOtherOccupantsInVehicle(void* vehicle) {
        if (!vehicle) {
            return 0;
        }
        void* playerPed = PlayerPed();
        int count = 0;
        __try {
            auto pool = *reinterpret_cast<std::uint8_t**>(
                GameAddr::Translate(GameAddr::CPools_ms_pPedPool));
            if (!pool) {
                return 0;
            }
            auto objects = *reinterpret_cast<std::uint8_t**>(pool + GameAddr::kCPool_m_pObjects);
            auto byteMap = *reinterpret_cast<std::uint8_t**>(pool + GameAddr::kCPool_m_byteMap);
            const int size = *reinterpret_cast<int*>(pool + GameAddr::kCPool_m_size);
            if (!objects || !byteMap || size <= 0 || size >= 100000) {
                return 0;
            }
            for (int i = 0; i < size; ++i) {
                if (byteMap[i] & GameAddr::kCPool_freeFlag) {
                    continue;
                }
                std::uint8_t* ped = objects +
                    static_cast<std::size_t>(i) * GameAddr::kSizeof_CPed;
                if (ped == playerPed) {
                    continue;
                }
                void* pedVehicle = *reinterpret_cast<void**>(
                    ped + GameAddr::CPed_m_pMyVehicle);
                if (pedVehicle == vehicle) {
                    ++count;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return -1;
        }
        return count;
    }

}
