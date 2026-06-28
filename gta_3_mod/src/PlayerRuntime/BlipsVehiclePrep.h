#pragma once
// PlayerRuntime: BlipsVehiclePrep (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    inline void ArmContactBlipCleanup() {
        // Classic-marker mode: the contact blip SYNC owns the radar (a blip =
        // "this contact has a playable mission"); the post-validation cleanup
        // would fight it every time a mission completes.
        if (Config::UseClassicMissionMarkers()) {
            return;
        }
        g_contactBlipCleanupFramesLeft = kContactBlipCleanupFrames;
        g_contactBlipCleanupApplied = 0;
    }

    inline void LaunchContactBlipCleanupStub() {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }

        std::uint8_t stub[128] = {};
        std::size_t n = 0;
        constexpr std::size_t kContactBlipCount =
            sizeof(kContactBlipGlobalOffsets) / sizeof(kContactBlipGlobalOffsets[0]);
        for (std::uint16_t offset : kContactBlipGlobalOffsets) {
            stub[n++] = 0x64;
            stub[n++] = 0x01;
            AppendScmGlobalVar(stub, &n, offset);
        }
        stub[n++] = 0x4E;
        stub[n++] = 0x00;

        CRunningScript* thread = LaunchScmStub(kClearContactBlipsStubOffset,
                                               stub,
                                               n,
                                               ScmSlots::ClearContactBlipsBytes,
                                               "contact-blip-cleanup");
        if (thread) {
            Logger::Log("Contact blip cleanup queued (%zu blips)", kContactBlipCount);
        }
    }

    // ===== Classic-marker mode: contact blip sync ===========================
    // A contact's radar blip is shown exactly while that contact has a
    // PLAYABLE mission (AP unlock item received, predecessors validated, not
    // yet done). Coordinates/sprites/global offsets lifted verbatim from the
    // compiled main_ap.scm blip-preload block (data\main_ap.ir2).
    struct ContactBlipDef {
        ScriptRuntime::MissionBucket bucket;
        float        x, y, z;
        std::uint8_t sprite;
        std::uint16_t globalOffset;
    };
    inline constexpr ContactBlipDef kContactBlipDefs[] = {
        {ScriptRuntime::MissionBucket::Luigi,         892.8f,  -425.8f,   13.9f, 13, 844},
        {ScriptRuntime::MissionBucket::Joey,         1191.7f,  -870.0f, -100.0f, 10, 848},
        {ScriptRuntime::MissionBucket::Toni,         1219.6f,  -321.0f, -100.0f, 19, 852},
        {ScriptRuntime::MissionBucket::Frankie,      1455.7f,  -187.2f, -100.0f, 16, 856},
        {ScriptRuntime::MissionBucket::Diablo,        938.4f,  -230.5f, -100.0f,  8, 860},
        {ScriptRuntime::MissionBucket::Asuka,         523.6f,  -639.4f,   16.6f,  1, 864},
        {ScriptRuntime::MissionBucket::Kenji,         459.1f, -1413.0f,   26.1f, 11, 868},
        {ScriptRuntime::MissionBucket::Ray,            38.8f,  -725.4f, -100.0f, 15, 872},
        {ScriptRuntime::MissionBucket::DonaldLove,     86.1f, -1548.7f,   28.2f,  6, 876},
        {ScriptRuntime::MissionBucket::Yardie,        120.7f,  -272.1f,   16.1f, 12, 880},
        {ScriptRuntime::MissionBucket::AsukaSuburban, -363.7f,  246.1f,   60.0f,  3, 884},
        {ScriptRuntime::MissionBucket::Hood,          -443.5f,    -6.1f,    3.8f,  9, 892},
    };
    constexpr int kContactBlipDefCount =
        static_cast<int>(sizeof(kContactBlipDefs) / sizeof(kContactBlipDefs[0]));
    // -1 unknown (resync), 0 hidden, 1 shown. Reset whenever the script
    // engine goes away (engine restarts wipe every blip handle).
    inline std::int8_t g_contactBlipShown[kContactBlipDefCount] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    inline int g_contactBlipSyncFrames = 0;
    constexpr int kContactBlipSyncIntervalFrames = 45;
    constexpr std::uint32_t kContactBlipSyncStubOffset =
        ScmSlots::ContactBlipSyncOffset;

    inline bool LaunchContactBlipSetStub(const ContactBlipDef& def, bool show) {
        std::uint8_t stub[128] = {};
        std::size_t n = 0;
        stub[n++] = 0x64; stub[n++] = 0x01;          // REMOVE_BLIP &g
        AppendScmGlobalVar(stub, &n, def.globalOffset);
        if (show) {
            stub[n++] = 0xA7; stub[n++] = 0x02;      // ADD_SPRITE_BLIP_FOR_CONTACT_POINT
            AppendScmFloat(stub, &n, def.x);
            AppendScmFloat(stub, &n, def.y);
            AppendScmFloat(stub, &n, def.z);
            stub[n++] = 0x04; stub[n++] = def.sprite; //   sprite (INT8)
            AppendScmGlobalVar(stub, &n, def.globalOffset); // -> handle
        }
        stub[n++] = 0x4E; stub[n++] = 0x00;          // TERMINATE_THIS_SCRIPT
        return LaunchScmStub(kContactBlipSyncStubOffset,
                             stub,
                             n,
                             ScmSlots::ContactBlipSyncBytes,
                             "contact-blip-sync") != nullptr;
    }

    inline void TryApplyContactBlipSync() {
        if (!Config::UseClassicMissionMarkers() || !RunState::IsRunLive()) {
            return;
        }
        if (ScriptRuntime::AlreadyRunningAMissionScript() ||
            IsIntroSequencePendingValidation()) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            for (std::int8_t& state : g_contactBlipShown) {
                state = -1; // engine gone: every handle is dead, resync later
            }
            return;
        }
        if (MenuMgr::CurrentPage() != MenuPage::None || !PlayerPed()) {
            return;
        }
        if (++g_contactBlipSyncFrames < kContactBlipSyncIntervalFrames) {
            return;
        }
        g_contactBlipSyncFrames = 0;
        // One change per tick: each change runs a tiny helper script and the
        // playable set only moves on item receipt / mission completion.
        for (int i = 0; i < kContactBlipDefCount; ++i) {
            const ContactBlipDef& def = kContactBlipDefs[i];
            const std::int8_t desired =
                ScriptRuntime::HasPlayableMissionInBucket(def.bucket) ? 1 : 0;
            if (g_contactBlipShown[i] == desired) {
                continue;
            }
            if (LaunchContactBlipSetStub(def, desired == 1)) {
                g_contactBlipShown[i] = desired;
                Logger::Log("Contact blip sync: bucket=%d -> %s",
                            static_cast<int>(def.bucket),
                            desired ? "shown" : "hidden");
            }
            return;
        }
    }

    // Toasts queued from contexts that cannot start helper scripts directly
    // (the StartNewScript hook, the menu transition handlers).
    inline void TryApplyMarkerBlockedToast() {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        if (g_markerBlockedToastPending) {
            g_markerBlockedToastPending = false;
            TextOverrides::SetBridgeToast(g_markerBlockedToast);
            LaunchPrintNowStub("APBRG", 5000);
        }
        if (g_offlineRunToastPending) {
            g_offlineRunToastPending = false;
            TextOverrides::SetBridgeToast("ARCHIPELAGO OFFLINE: RUN NOT STARTED");
            LaunchPrintNowStub("APBRG", 5000);
        }
    }

    inline void TryApplyContactBlipCleanup() {
        if (g_contactBlipCleanupFramesLeft <= 0) {
            return;
        }

        if ((g_contactBlipCleanupApplied % kContactBlipCleanupStride) == 0) {
            LaunchContactBlipCleanupStub();
        }
        ++g_contactBlipCleanupApplied;
        --g_contactBlipCleanupFramesLeft;
    }

    // ===== Service blips (Pay'n'Spray / Bomb shop) =====
    // The "repères tout sauf missions": vanilla preloads the persistent shop
    // blips at new-game, but our AP boot bypasses that init, so the radar comes
    // up bare. We re-add them ASI-side, gated by island unlock so they appear
    // "au fur et a mesure" (Portland now, Staunton/Shoreside as their unlock_*
    // items arrive). Coords lifted from the SET_GARAGE defs in main.sc; sprites
    // read verbatim from the compiled bytecode at those coords (18=Pay'n'Spray,
    // 2=Bomb). Handle stored into &1600 — the exact global vanilla reused for
    // these permanent shop blips — and discarded (service blips are never
    // removed, so a single scratch handle is enough).
    struct ServiceBlipDef {
        int          island;   // 0=Portland, 1=Staunton(+commercial), 2=Shoreside
        float        x, y, z;
        std::uint8_t sprite;
    };
    inline constexpr ServiceBlipDef kServiceBlipDefs[] = {
        {0,   925.0f,  -359.5f, -100.0f, 18}, // Pay'n'Spray  Portland
        {0,  1282.0f,  -104.0f, -100.0f,  2}, // Bomb shop    Portland
        {1,   379.0f,  -493.8f, -100.0f, 18}, // Pay'n'Spray  Staunton
        {1,   380.0f,  -577.0f, -100.0f,  2}, // Bomb shop    Staunton
        {2, -1128.0f,    32.5f, -100.0f, 18}, // Pay'n'Spray  Shoreside
        {2, -1078.9f,    58.0f, -100.0f,  2}, // Bomb shop    Shoreside
    };
    constexpr int kServiceBlipDefCount =
        static_cast<int>(sizeof(kServiceBlipDefs) / sizeof(kServiceBlipDefs[0]));
    constexpr std::uint16_t kServiceBlipScratchGlobalOffset = 1600;
    // false = not placed, true = placed this engine-life. Reset on engine loss.
    inline bool g_serviceBlipShown[kServiceBlipDefCount] = {};
    inline int g_serviceBlipSyncFrames = 0;
    constexpr int kServiceBlipSyncIntervalFrames = 30;
    constexpr std::uint32_t kServiceBlipStubOffset =
        ScmSlots::ServiceBlipOffset;

    inline bool LaunchServiceBlipAddStub(const ServiceBlipDef& def) {
        std::uint8_t stub[128] = {};
        std::size_t n = 0;
        stub[n++] = 0xA8; stub[n++] = 0x02;      // ADD_SPRITE_BLIP_FOR_COORD
        AppendScmFloat(stub, &n, def.x);
        AppendScmFloat(stub, &n, def.y);
        AppendScmFloat(stub, &n, def.z);
        stub[n++] = 0x04; stub[n++] = def.sprite; //   sprite (INT8)
        AppendScmGlobalVar(stub, &n, kServiceBlipScratchGlobalOffset); // -> handle
        stub[n++] = 0x4E; stub[n++] = 0x00;      // TERMINATE_THIS_SCRIPT
        return LaunchScmStub(kServiceBlipStubOffset,
                             stub,
                             n,
                             ScmSlots::ServiceBlipBytes,
                             "service-blip") != nullptr;
    }

    inline void TryApplyServiceBlipSync() {
        if (!Config::UseClassicMissionMarkers() || !RunState::IsRunLive()) {
            return;
        }
        if (ScriptRuntime::AlreadyRunningAMissionScript() ||
            IsIntroSequencePendingValidation()) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            for (bool& placed : g_serviceBlipShown) {
                placed = false; // engine gone: handles wiped, replace later
            }
            return;
        }
        if (MenuMgr::CurrentPage() != MenuPage::None || !PlayerPed()) {
            return;
        }
        if (++g_serviceBlipSyncFrames < kServiceBlipSyncIntervalFrames) {
            return;
        }
        g_serviceBlipSyncFrames = 0;
        const int island = ScriptRuntime::ApRegionMapState();
        // One placement per tick (each runs a tiny helper script).
        for (int i = 0; i < kServiceBlipDefCount; ++i) {
            const ServiceBlipDef& def = kServiceBlipDefs[i];
            if (g_serviceBlipShown[i] || def.island > island) {
                continue;
            }
            if (LaunchServiceBlipAddStub(def)) {
                g_serviceBlipShown[i] = true;
                Logger::Log("Service blip placed: sprite=%d island=%d (%.1f,%.1f)",
                            static_cast<int>(def.sprite), def.island, def.x, def.y);
            }
            return;
        }
    }

    inline void LaunchAddScoreStub(int amount) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            Logger::Log("Cash item: script engine not live, reward ignored");
            return;
        }

        const long clamped =
            (std::max)(-32768L, (std::min)(32767L, static_cast<long>(amount)));
        std::uint8_t stub[16] = {};
        std::size_t n = 0;
        stub[n++] = 0x09; stub[n++] = 0x01; // ADD_SCORE
        AppendScmPlayerHandle(stub, &n);
        AppendScmInt16(stub, &n, static_cast<std::int16_t>(clamped));
        stub[n++] = 0x4E; stub[n++] = 0x00; // TERMINATE_THIS_SCRIPT

        CRunningScript* s = LaunchScmStub(kAddScoreStubOffset,
                                          stub,
                                          n,
                                          ScmSlots::AddScoreBytes,
                                          "cash-item");
        if (s) {
            Logger::Log("Cash item: queued ADD_SCORE %ld", clamped);
        }
    }

    inline bool LaunchMissionVehiclePrepStub(
        const MissionSpawn& mission,
        bool makePlayerSafe) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            Logger::Log("Mission vehicle prep: script engine not live");
            return false;
        }
        void* playerVehicle = PlayerVehicle();
        if (!playerVehicle) {
            return false;
        }

        const SpawnPoint parking = MissionVehicleParking(mission);

        // NO area sweep here: a CLEAR_AREA_OF_CARS around the parking slot ran
        // concurrently with the mission script creating its own set-piece
        // vehicles (LUIGI2's club staging overlaps the slot) and deleting one
        // from under it crashed the engine positioning a NULL entity
        // (0xC0000005 @ 0x4755C0). If an earlier launch left a car on the
        // slot, the engine's collision push separates the overlap — messy but
        // harmless, unlike the sweep.
        std::uint8_t stub[128] = {};
        std::size_t n = 0;

        stub[n++] = 0xC1; stub[n++] = 0x03; // STORE_CAR_PLAYER_IS_IN_NO_SAVE
        AppendScmPlayerHandle(stub, &n);
        AppendScmLocalVar0(stub, &n);

        stub[n++] = 0x2A; stub[n++] = 0x01; // WARP_PLAYER_FROM_CAR_TO_COORD
        AppendScmPlayerHandle(stub, &n);
        AppendScmFloat(stub, &n, mission.spawn.position.x);
        AppendScmFloat(stub, &n, mission.spawn.position.y);
        AppendScmFloat(stub, &n, mission.spawn.position.z);

        stub[n++] = 0xAB; stub[n++] = 0x00; // SET_CAR_COORDINATES
        AppendScmLocalVar0(stub, &n);
        AppendScmFloat(stub, &n, parking.position.x);
        AppendScmFloat(stub, &n, parking.position.y);
        AppendScmFloat(stub, &n, parking.position.z);

        stub[n++] = 0x75; stub[n++] = 0x01; // SET_CAR_HEADING
        AppendScmLocalVar0(stub, &n);
        AppendScmFloat(stub, &n, parking.heading);

        if (makePlayerSafe) {
            stub[n++] = 0xEF; stub[n++] = 0x03; // MAKE_PLAYER_SAFE_FOR_CUTSCENE
            AppendScmPlayerHandle(stub, &n);
        }

        stub[n++] = 0x4E; stub[n++] = 0x00; // TERMINATE_THIS_SCRIPT

        CRunningScript* s = LaunchScmStub(kMissionVehiclePrepStubOffset,
                                          stub,
                                          n,
                                          ScmSlots::MissionVehiclePrepBytes,
                                          "mission-vehicle-prep");
        if (!s) {
            Logger::Log("Mission vehicle prep: failed to queue stub");
            return false;
        }

        Logger::Log("Mission vehicle prep queued: mission=(%.1f,%.1f,%.1f) parked=(%.1f,%.1f,%.1f) safe=%d",
                    mission.spawn.position.x, mission.spawn.position.y, mission.spawn.position.z,
                    parking.position.x, parking.position.y, parking.position.z,
                    makePlayerSafe ? 1 : 0);
        return true;
    }

    inline bool TryPrepareVehicleForMissionLaunch(
        int actualIndex,
        ScriptRuntime::MissionLaunchVariant variant,
        bool makePlayerSafe) {
        void* playerVehicle = PlayerVehicle();
        if (actualIndex == 23) {
            Logger::Log("Mission vehicle prep skipped: actual=%d keeps passenger state across failed replays", actualIndex);
            return false;
        }
        const MissionSpawn* mission = FindMissionSpawn(actualIndex, variant);
        if (!mission || !playerVehicle) {
            return false;
        }
        const int otherOccupants = CountOtherOccupantsInVehicle(playerVehicle);
        if (otherOccupants < 0) {
            Logger::Log("Mission vehicle prep skipped: actual=%d occupant scan failed", actualIndex);
            return false;
        }
        // Mid-mission hot-swaps (makePlayerSafe == "we just aborted a live
        // mission") must not strand a mission passenger by warping the player
        // out from under them. A FRESH launch has no mission passenger — any
        // occupant is a leftover (taxi fare, post-GML 8-Ball) and the player
        // must still arrive on foot, so the prep proceeds.
        if (otherOccupants > 0 && makePlayerSafe) {
            Logger::Log("Mission vehicle prep skipped: actual=%d vehicle still has %d other occupant(s) after abort", actualIndex, otherOccupants);
            return false;
        }
        return LaunchMissionVehiclePrepStub(*mission, makePlayerSafe);
    }

    inline bool LaunchPlayerVehicleTeleportStub(
        const CVector& pos,
        const float* heading,
        const char* tag) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            Logger::Log("%s: script engine not live, vehicle teleport ignored", tag);
            return false;
        }
        if (!PlayerVehicle()) {
            Logger::Log("%s: player not in a vehicle, ignoring", tag);
            return false;
        }

        std::uint8_t stub[96] = {};
        std::size_t n = 0;

        stub[n++] = 0xC1; stub[n++] = 0x03; // STORE_CAR_PLAYER_IS_IN_NO_SAVE
        stub[n++] = 0x02; stub[n++] = 0x10; stub[n++] = 0x02; // player handle &528
        AppendScmLocalVar0(stub, &n);                          // -> local car handle

        stub[n++] = 0xF5; stub[n++] = 0x01;                   // GET_PLAYER_CHAR
        AppendScmPlayerHandle(stub, &n);
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);

        stub[n++] = 0x2A; stub[n++] = 0x01;                   // WARP_PLAYER_FROM_CAR_TO_COORD
        AppendScmPlayerHandle(stub, &n);
        AppendScmFloat(stub, &n, pos.x);
        AppendScmFloat(stub, &n, pos.y);
        AppendScmFloat(stub, &n, pos.z);

        stub[n++] = 0xAB; stub[n++] = 0x00;                   // SET_CAR_COORDINATES
        AppendScmLocalVar0(stub, &n);
        AppendScmFloat(stub, &n, pos.x);
        AppendScmFloat(stub, &n, pos.y);
        AppendScmFloat(stub, &n, pos.z);

        if (heading) {
            stub[n++] = 0x75; stub[n++] = 0x01;               // SET_CAR_HEADING
            AppendScmLocalVar0(stub, &n);
            AppendScmFloat(stub, &n, *heading);
        }

        stub[n++] = 0x6A; stub[n++] = 0x03;                   // WARP_CHAR_INTO_CAR
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);
        AppendScmLocalVar0(stub, &n);

        stub[n++] = 0x4E; stub[n++] = 0x00;                   // TERMINATE_THIS_SCRIPT

        CRunningScript* s = LaunchScmStub(kVehicleTeleportStubOffset,
                                          stub,
                                          n,
                                          ScmSlots::VehicleTeleportBytes,
                                          tag);
        if (!s) {
            Logger::Log("%s: failed to queue vehicle teleport stub", tag);
            return false;
        }

        if (heading) {
            Logger::Log("%s: queued vehicle teleport to (%.1f,%.1f,%.1f) heading=%.1f",
                        tag, pos.x, pos.y, pos.z, *heading);
        } else {
            Logger::Log("%s: queued vehicle teleport to (%.1f,%.1f,%.1f)",
                        tag, pos.x, pos.y, pos.z);
        }
        return true;
    }

    inline bool TryTeleportVehicleAware(
        const CVector& pos,
        const float* heading,
        const char* tag,
        bool allowPedFallback) {
        if (PlayerVehicle()) {
            if (LaunchPlayerVehicleTeleportStub(pos, heading, tag)) {
                return true;
            }
            if (!allowPedFallback) {
                return false;
            }
        } else if (!allowPedFallback) {
            Logger::Log("%s: player not in a vehicle, ignoring", tag);
            return false;
        }

        void* ped = PlayerPed();
        if (!ped) {
            Logger::Log("%s: no player ped, ignoring", tag);
            return false;
        }

        __try {
            auto teleport = reinterpret_cast<TeleportFn>(
                GameAddr::Translate(GameAddr::CPed_Teleport));
            teleport(ped, pos);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("%s: CPed::Teleport raised 0x%08lX", tag, GetExceptionCode());
            return false;
        }

        Logger::Log("%s: teleported player on foot to (%.1f,%.1f,%.1f)",
                    tag, pos.x, pos.y, pos.z);
        return true;
    }

}
