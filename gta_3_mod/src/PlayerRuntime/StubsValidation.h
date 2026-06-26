#pragma once
// PlayerRuntime: StubsValidation (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    // Used only for Give Me Liberty's fail path. The vanilla script calls
    // RESTART_CRITICAL_MISSION, then disables player control while it waits for
    // IS_PLAYER_PLAYING to become true again. We reset WBState from C++ and use
    // this tiny SCM helper for the script-side state: cancel the pending restart
    // override and restore control before the retry launches.
    constexpr std::uint32_t kGiveMeLibertyFailFastRestoreStubOffset =
        ScmSlots::GmlFailFastRestoreOffset;

    inline void LaunchGiveMeLibertyFailFastRestoreStub() {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        const std::uint8_t stub[] = {
            0xF6, 0x01,             // CANCEL_OVERRIDE_RESTART (0x01F6)
            0xB4, 0x01,             // SET_PLAYER_CONTROL (0x01B4)
            0x02, 0x10, 0x02,       //   arg1: global var &528 (player)
            0x04, 0x01,             //   arg2: INT8 enabled=1
            0x4E, 0x00,             // TERMINATE_THIS_SCRIPT
        };
        CRunningScript* s = LaunchScmStub(kGiveMeLibertyFailFastRestoreStubOffset,
                                          stub,
                                          ScmSlots::GmlFailFastRestoreBytes,
                                          "GML fail-fast restore");
        if (s) {
            Logger::Log("GML fail-fast restore stub queued: ip=%u",
                        kGiveMeLibertyFailFastRestoreStubOffset);
        }
    }

    inline void LaunchTestPrintStub() {
        CRunningScript* stub = LaunchScmStub(kTestPrintStubOffset,
                                             kTestPrintStub,
                                             ScmSlots::PrintNowBytes,
                                             "test-print");
        if (stub) {
            Logger::Log("Test-print stub queued: ip=%u (PRINT_NOW 'TEST_AP')",
                        kTestPrintStubOffset);
        }
    }

    // Generic PRINT_NOW <key> 3000ms flag=1 launcher: same stub as the test
    // key but with a caller-supplied (up to) 8-byte GXT key, so any feature can
    // pop an on-screen toast that also lands in the Brief log, routed through
    // TextOverrides. Reuses the test stub's ScriptSpace slot — notifications
    // are transient and never overlap within a single frame.
    inline void LaunchPrintNowStub(const char* gxtKey, int durationMs = 3000) {
        // The PRINT_NOW stub runs as a real CRunningScript: the game's
        // StartNewScript() pops a node off the idle-script list and unlinks it
        // through CRunningScript::RemoveFromList (0x00438FB0). Before the SCM
        // engine is live (e.g. during the auto-boot into the AP freeroam, or any
        // bridge toast that fires while the world is still loading) that idle
        // list head is null, so RemoveFromList dereferences null+4 and crashes
        // at 0x00438FB1 with ECX=0. Bail out until the engine is up — dropping a
        // boot-time toast is harmless and avoids the crash for every caller.
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        const std::uint16_t dur = static_cast<std::uint16_t>(
            durationMs < 1 ? 1 : (durationMs > 60000 ? 60000 : durationMs));
        std::uint8_t stub[17] = {
            0xBC, 0x00,
            0, 0, 0, 0, 0, 0, 0, 0,  // 8-byte GXT key, filled below
            0x05, static_cast<std::uint8_t>(dur & 0xFF),
                  static_cast<std::uint8_t>((dur >> 8) & 0xFF), // INT16 duration ms
            0x04, 0x01,              // INT8 flag 1 (also append to Brief log)
            0x4E, 0x00,              // TERMINATE_THIS_SCRIPT
        };
        std::size_t n = std::strlen(gxtKey);
        if (n > 8) { n = 8; }
        std::memcpy(stub + 2, gxtKey, n);

        CRunningScript* s = LaunchScmStub(kTestPrintStubOffset,
                                          stub,
                                          ScmSlots::PrintNowBytes,
                                          "PRINT_NOW");
        if (s) {
            Logger::Log("PRINT_NOW stub queued: key='%.8s' ip=%u",
                        gxtKey, kTestPrintStubOffset);
        }
    }

    // Debug hotkey: pressing P (edge-triggered) fires the PRINT_NOW stub once.
    // The notification appears for ~3 s on screen and lands in the Brief
    // pause-menu page. Polled from the per-frame CMenuManager::Process hook.
    inline bool g_testKeyDown = false;  // TryFireTestKey -> debug/DebugHotkeys.h

    // ===== Trap hotkey (M): 3-star police wanted level =====
    // ALTER_WANTED_LEVEL &528 3 + TERMINATE_THIS_SCRIPT bytecode stub. Opcode
    // 0x010E resolves CWorld::Players[&528] and calls CWanted::SetWantedLevel(3)
    // (verified: handler 0x43E098 -> 0x4F31B0 -> CWanted::SetWantedLevel at
    // 0x4ADAC0), which spawns the police response — an Archipelago "trap". The
    // player handle global is &528 (the player index, as the other stubs use).
    // Encoding mirrors the test stub: opcode, global-var arg (type 0x02 + 2-byte
    // offset), INT8 arg (type 0x04 + 1 byte). Placed in a fresh 16-byte slot
    // below the test-print stub (kSizeMainScript-64).
    constexpr std::uint32_t kWantedTrapStubOffset =
        ScmSlots::WantedOffset;
    constexpr std::uint8_t kWantedTrapStub[] = {
        0x0E, 0x01,             // ALTER_WANTED_LEVEL_NO_DROP (0x010E): raises only
        0x02, 0x10, 0x02,       //   arg1: global var &528 (player)
        0x04, 0x03,             //   arg2: INT8 wanted level 3
        0x4E, 0x00,             // TERMINATE_THIS_SCRIPT
    };
    static_assert(sizeof(kWantedTrapStub) == 9,
                  "Wanted-trap stub bytecode size mismatch");

    inline void LaunchWantedTrapStub() {
        CRunningScript* stub = LaunchScmStub(kWantedTrapStubOffset,
                                             kWantedTrapStub,
                                             ScmSlots::WantedBytes,
                                             "wanted-trap");
        if (stub) {
            Logger::Log("Wanted-trap stub queued: ip=%u (ALTER_WANTED_LEVEL 3)",
                        kWantedTrapStubOffset);
        }
    }

    inline bool g_trapKeyDown = false;  // TryFireTrapKey -> debug/DebugHotkeys.h

    // ===== Clear-wanted hotkey (C): wipe the police wanted level =====
    // Must use CLEAR_WANTED_LEVEL (opcode 0x0110), NOT the trap's
    // ALTER_WANTED_LEVEL_NO_DROP (0x010E): the "NO_DROP" variant refuses to
    // LOWER the level, so writing 0 with it does nothing (that was the bug).
    // CLEAR_WANTED_LEVEL drops the stars and calls off the active police
    // response, like the NOMOREPOLICEPLEASE cheat. Takes only the player arg.
    // Reuses the wanted-trap's 16-byte slot (they never run in the same frame).
    inline void LaunchClearWantedStub() {
        const std::uint8_t stub[] = {
            0x10, 0x01,             // CLEAR_WANTED_LEVEL (opcode 0x0110)
            0x02, 0x10, 0x02,       //   arg: global var &528 (player)
            0x4E, 0x00,             // TERMINATE_THIS_SCRIPT
        };
        CRunningScript* s = LaunchScmStub(kWantedTrapStubOffset,
                                          stub,
                                          ScmSlots::WantedBytes,
                                          "clear-wanted");
        if (s) {
            Logger::Log("Clear-wanted stub queued: ip=%u (ALTER_WANTED_LEVEL 0)",
                        kWantedTrapStubOffset);
        }
    }

    inline bool g_clearWantedKeyDown = false;  // TryFireClearWantedKey -> debug/DebugHotkeys.h

    // ===== Vehicle damage traps (F11 = smoking, F12 = on fire) =====
    // The player's current car is CPed::m_pMyVehicle; dropping its
    // CVehicle::m_fHealth lets CAutomobile::ProcessControl react: heavy engine
    // smoke at low health, flames near zero. Only acts while in a vehicle.
    constexpr float kVehicleSmokeHealth = 250.0f; // visibly smoking, still driveable
    constexpr float kVehicleFireHealth  = 0.0f;   // catches fire (then burns out)

    inline bool SetPlayerVehicleHealth(float health, const char* tag) {
        void* veh = PlayerVehicle();
        if (!veh) {
            Logger::Log("%s: player not in a vehicle, ignoring", tag);
            return false;
        }
        bool ok = true;
        __try {
            *reinterpret_cast<float*>(
                static_cast<std::uint8_t*>(veh) + GameAddr::CVehicle_m_fHealth) = health;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("%s: writing vehicle health raised 0x%08lX", tag, GetExceptionCode());
            ok = false;
        }
        if (ok) {
            Logger::Log("%s: vehicle health set to %.0f", tag, health);
        }
        return ok;
    }

    inline bool g_smokeKeyDown = false;  // TryFireVehicleSmokeKey -> debug/DebugHotkeys.h
    inline bool g_fireKeyDown  = false;  // TryFireVehicleFireKey  -> debug/DebugHotkeys.h

    // ===== Island unlocking (automatic, no hotkey) =====
    // PushMapStateToScript pushes EffectiveMapState() into the SCM's unused_2
    // global every frame; the mission_start polling loop then runs the
    // DELETE_OBJECT + SWITCH_ROADS_ON unlock sequence for the newly opened
    // island. EffectiveMapState rises when a contact that lives on that island
    // is unlocked (its mission OR its AP check) or the AP region item arrives —
    // see PlayerRuntime::ContactUnlockMapState. The old F7 manual key and the
    // 3-minute third-island timer were retired.

    // ===== Unlock character hotkey (F2): reveal the next contact =====
    // Raises RunState::UnlockedCharacters by one (capped at the total contact
    // count). The selector shows a contact's missions only once its character
    // is unlocked, and an island only appears once it has an unlocked contact.
    // A fresh run starts with just Luigi unlocked.
    inline bool g_unlockCharacterKeyDown = false;  // TryFireUnlockCharacterKey -> debug/DebugHotkeys.h

    // ===== Unlock everything hotkey (F4, debug): all contacts + all islands ====
    // One press: UnlockedCharacters -> full contact count and map_state -> 2, so
    // every mission is selectable and the inter-island bridges drop next frame
    // (PushMapStateToScript). Bypasses F7's progressive third-island delay.
    inline bool g_unlockEverythingKeyDown = false;  // TryFireUnlockEverythingKey -> debug/DebugHotkeys.h

    // Tear down the active mission without relocating the player (defined after
    // the cancel-restart stub it relies on).
    inline void EndMissionInPlace();

    // Debug key (validate_mission): mark the last-launched mission validated and
    // end it in place — Claude stays where he is, in his vehicle. Stands in for
    // the real pass-detection until that lands.
    inline bool g_validateMissionKeyDown = false;  // TryFireValidateMissionKey -> debug/DebugHotkeys.h

    // Debug key (objective_teleport): cycle-warp through the current mission's
    // recorded positions (positions.ini). Defined after the recorder.
    inline void TryFireObjectiveTeleportKey();
    inline void ArmContactBlipCleanup();
    inline void TryApplyContactBlipCleanup();
    inline bool TryApplyStoryUnlockForMission(int actualMissionIndex);
    inline void TrySyncStoryUnlockState();

    inline void ValidateSyntheticMissionComplete(const char* syntheticKey,
                                                 int mission,
                                                 const char* reason) {
        if (!syntheticKey || syntheticKey[0] == 0) {
            return;
        }

        FlushMoneyToRunState(reason ? reason : "synthetic mission validated", false);
        ArmContactBlipCleanup();

        OnMissionExit(mission,
                      syntheticKey,
                      MissionExitReason::Passed,
                      reason ? reason : "intro/GML validated");

        if (!RunState::IsSyntheticMissionValidated(syntheticKey)) {
            RunState::ValidateSyntheticMission(syntheticKey);
            const char* loc = ScriptRuntime::SyntheticMissionLocationId(syntheticKey);
            if (loc) {
                ApState::AddLocationChecked(loc);
            }
            LaunchPrintNowStub("APVALID");
            Logger::Log("Mission %d validated synthetic key='%s' (%s)",
                        mission, syntheticKey,
                        reason ? reason : "synthetic mission complete");
        }
    }

    // ===== Position recorder (² key): batch-save player positions per mission =====
    //
    // Each press appends Claude's current position (and heading) to
    // III.MissionSelector.positions.ini, grouped under the mission that was
    // launched (or [freeroam] when none). The file is human-editable so we can
    // refine precise objective coordinates per mission to drive a future warp.
    struct RecordedPos {
        float x;
        float y;
        float z;
        float heading;
    };
    inline std::map<int, std::vector<RecordedPos>> g_recordedPositions;
    inline bool g_recordedLoaded = false;

    inline std::string PositionsFilePath() {
        return PluginPaths::InGameDir("III.MissionSelector.positions.ini");
    }

    // Read Claude's facing as a GTA heading in degrees (0 = +Y, clockwise) from
    // his matrix forward ("at") vector at ped+0x24 (matrix base 0x04 + at 0x20).
    inline bool TryReadPlaceableHeading(void* entity, float* out) {
        if (!entity) {
            return false;
        }
        __try {
            const float* at = reinterpret_cast<const float*>(
                static_cast<std::uint8_t*>(entity) + 0x24);
            float deg = std::atan2(-at[0], at[1]) * 57.2957795f;
            if (deg < 0.0f) {
                deg += 360.0f;
            }
            *out = deg;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
    }

    inline bool TryReadPlayerHeading(float* out) {
        return TryReadPlaceableHeading(PlayerPed(), out);
    }

    inline bool TryReadVehicleHeading(float* out) {
        return TryReadPlaceableHeading(PlayerVehicle(), out);
    }

    inline void LoadRecordedPositions() {
        g_recordedPositions.clear();
        std::ifstream in(PositionsFilePath());
        g_recordedLoaded = true;
        if (!in.is_open()) {
            return;
        }
        std::string line;
        int current = -1;
        while (std::getline(in, line)) {
            std::size_t a = line.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) {
                continue;
            }
            if (line[a] == ';' || line[a] == '#') {
                continue;
            }
            if (line[a] == '[') {
                if (line.find("freeroam") != std::string::npos) {
                    current = -1;
                } else {
                    int n = -1;
                    if (std::sscanf(line.c_str() + a, "[mission %d", &n) == 1) {
                        current = n;
                    }
                }
                continue;
            }
            RecordedPos p{};
            const int got = std::sscanf(line.c_str() + a, "%f , %f , %f , %f",
                                        &p.x, &p.y, &p.z, &p.heading);
            if (got >= 3) {
                g_recordedPositions[current].push_back(p);
            }
        }
    }

    inline void SaveRecordedPositions() {
        FILE* f = nullptr;
        fopen_s(&f, PositionsFilePath().c_str(), "w");
        if (!f) {
            Logger::Log("Position recorder: failed to write '%s'",
                        PositionsFilePath().c_str());
            return;
        }
        std::fprintf(f, "; MissionSelector recorded vehicle positions (press the configured key to capture)\n");
        std::fprintf(f, "; Grouped per launched mission. Format: x, y, z, heading\n\n");
        for (const auto& group : g_recordedPositions) {
            if (group.first < 0) {
                std::fprintf(f, "[freeroam]\n");
            } else {
                const ScriptRuntime::VisibleMissionEntry* e =
                    ScriptRuntime::FindVisibleMissionByActualIndex(group.first);
                std::fprintf(f, "[mission %d%s%s]\n", group.first,
                             e ? " " : "", e ? e->displayKey : "");
            }
            for (const RecordedPos& p : group.second) {
                std::fprintf(f, "%.2f, %.2f, %.2f, %.1f\n", p.x, p.y, p.z, p.heading);
            }
            std::fprintf(f, "\n");
        }
        std::fclose(f);
    }

    inline bool g_recordPosKeyDown = false;  // TryFireRecordPositionKey -> debug/DebugHotkeys.h

    // Auto-validation watch: a selector-launched mission keeps the engine's
    // bAlreadyRunningAMissionScript flag true for its whole run. When that flag
    // drops (the mission script terminated) AND the player is still alive, the
    // mission was passed (not a fatal fail), so we mark it validated → its (V)
    // shows in the selector. A short debounce avoids reacting to a 1-frame
    // flicker, and a death (alive==false) re-arms instead of validating so a
    // wasted→restart isn't mistaken for a pass.
    inline bool g_onMissionScriptSeen = false;
    inline int  g_missionEndedFrames = 0;
    // CStats::MissionsPassed sampled the frame the mission script went live. The
    // mission is "passed" iff the counter has incremented by the time the script
    // terminates (REGISTER_MISSION_PASSED, opcode 0x318). Failures never bump it.
    inline int  g_missionPassedSnapshot = 0;

    inline void ResetMissionValidationWatchState() {
        g_onMissionScriptSeen = false;
        g_missionEndedFrames = 0;
    }

    // SEH-isolated reads (MSVC forbids __try in a function that also has C++
    // objects needing unwinding, e.g. the std::string built for the AP check).
    inline int SafeReadAlreadyRunningMission() {  // 1/0, or -1 on fault
        __try {
            return ScriptRuntime::AlreadyRunningAMissionScript() ? 1 : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return -1;
        }
    }
    inline bool SafeReadPlayerAlive() {
        void* ped = PlayerPed();
        if (!ped) {
            return true;
        }
        __try {
            const float hp = *reinterpret_cast<float*>(
                static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_fHealth);
            return hp > 0.0f;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return true;
        }
    }

    inline bool TryApplyStoryUnlockForMission(int actualMissionIndex) {
        if (actualMissionIndex < 0) {
            return false;
        }

        char firstUnlockDisplayKey[9] = {};
        bool unlockedAnyBucket = false;
        for (const StoryMissionUnlockDef& unlock : kStoryMissionUnlocks) {
            if (unlock.triggerActualIndex != actualMissionIndex) {
                continue;
            }
            if (ScriptRuntime::IsCharacterUnlocked(unlock.bucket)) {
                continue;
            }

            RunState::UnlockBucket(static_cast<int>(unlock.bucket));
            Logger::Log("Story unlock: mission index=%d unlocked bucket=%d",
                        actualMissionIndex, static_cast<int>(unlock.bucket));
            unlockedAnyBucket = true;

            if (firstUnlockDisplayKey[0] == 0 &&
                unlock.unlockDisplayKey && unlock.unlockDisplayKey[0] != 0) {
                std::memset(firstUnlockDisplayKey, 0, sizeof(firstUnlockDisplayKey));
                std::memcpy(firstUnlockDisplayKey,
                            unlock.unlockDisplayKey,
                            std::min<std::size_t>(std::strlen(unlock.unlockDisplayKey), 8));
            }
        }

        if (!unlockedAnyBucket) {
            return false;
        }

        if (firstUnlockDisplayKey[0] == 0 &&
            !ScriptRuntime::TryGetMissionDisplayKeyByActualIndex(actualMissionIndex, firstUnlockDisplayKey)) {
            return false;
        }

        char toastKey[9] = {};
        TextOverrides::RegisterUnlockToast(toastKey, firstUnlockDisplayKey);
        LaunchPrintNowStub(toastKey);
        return true;
    }

    inline void TrySyncStoryUnlockState() {
        for (const StoryMissionUnlockDef& unlock : kStoryMissionUnlocks) {
            if (!RunState::IsMissionValidated(unlock.triggerActualIndex) &&
                !ScriptRuntime::IsMissionLocationChecked(unlock.triggerActualIndex)) {
                continue;
            }
            if (!RunState::IsBucketUnlocked(static_cast<int>(unlock.bucket))) {
                RunState::UnlockBucket(static_cast<int>(unlock.bucket));
            }
        }
    }

    inline void TryApplyMissionValidationWatch() {
        const int mission = RunState::LastLaunchedMission();
        if (mission < 0) {
            g_onMissionScriptSeen = false;
            g_missionEndedFrames = 0;
            return;
        }

        const int onMission = SafeReadAlreadyRunningMission();
        if (onMission < 0) {
            return;
        }
        if (onMission == 1) {
            if (!g_onMissionScriptSeen) {
                // First frame the mission script is live: baseline the pass counter.
                g_missionPassedSnapshot = GameAddr::ReadMissionsPassedOrZero();
            }
            g_onMissionScriptSeen = true;
            g_missionEndedFrames = 0;
            return;
        }
        if (!g_onMissionScriptSeen) {
            return; // mission script hasn't started yet
        }
        if (++g_missionEndedFrames < 5) {
            return; // debounce
        }
        g_missionEndedFrames = 0;

        const bool alive = SafeReadPlayerAlive();
        // A mission only counts as passed when REGISTER_MISSION_PASSED
        // (CStats::MissionsPassed) fired during the run. Soft fails (target lost,
        // time-out, area left) and deaths never bump it, so "still alive at the
        // end" is NOT a pass -- that old heuristic validated failed missions.
        const bool passed = GameAddr::ReadMissionsPassedOrZero() > g_missionPassedSnapshot;

        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        const int validationMission = RunState::PendingValidationMission();

        if (syntheticKey && syntheticKey[0] != 0) {
            // The intro/8-Ball sequence completes in two stages: the intro cutscene
            // ends (alive, not yet passed) and only its 8-Ball follow-up actually
            // calls REGISTER_MISSION_PASSED. Keep arming the follow-up while alive.
            if (!alive) {
                Logger::Log("Synthetic mission %d ended with player dead -> not validated, re-arming", mission);
                OnMissionExit(mission,
                              syntheticKey,
                              MissionExitReason::FailedDeath,
                              "player dead");
                g_onMissionScriptSeen = false;
                return;
            }
            if (ScriptRuntime::IsIntroSequenceSyntheticKey(syntheticKey) &&
                !g_introEightballStarted) {
                ArmIntroFollowupLaunchWatch();
                return;
            }
            if (!passed) {
                Logger::Log("Synthetic mission %d ended without REGISTER_MISSION_PASSED -> not validated, re-arming", mission);
                OnMissionExit(mission,
                              syntheticKey,
                              MissionExitReason::Failed,
                              "no mission passed increment");
                g_onMissionScriptSeen = false;
                return;
            }
            // Money: a plain FlushMoneyToRunState no-ops here because the intro
            // pending key is still set (it clears at the tail of this watch),
            // and the flush bails on IsIntroSequencePendingValidation(). Left
            // unflushed, RunState stays at the pre-intro $0, and next frame
            // TryApplyMoneyPersistence (which runs BEFORE this watch) takes the
            // once-per-session restore branch and writes that $0 over the cash
            // Claude just earned in GML/Luigi's Girls. Capture the live total
            // now and prime the restore latch so the persistence tick mirrors
            // it instead of wiping it.
            int introCash = 0;
            if (GameAddr::TryReadPlayerCash(&introCash)) {
                RunState::SetMoney(introCash);
                g_moneyRestoredThisSession = true;
                g_lastSyncedMoney = introCash;
                g_moneySyncFrames = 0;
                Logger::Log("Intro/GML money captured at validation: $%d", introCash);
            }
            // The EIGHT chain hands Claude back to free-roam but leaves him
            // FROZEN — player control off, widescreen on, cutscene camera — and
            // the $ONMISSION script global set, so he can't move and the classic
            // markers stay frozen until a restart. Nothing wired the free-roam
            // restore into the "chain ran straight through into LM1" completion
            // (only the GML split-exit ever used it), which is exactly this bug.
            // Run it now: re-enable control, drop widescreen, restore the
            // camera, clear $ONMISSION/fail/wbstate, and eject Claude from the
            // mission car where he sits ("sortie de mission" -> on bouge).
            if (ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(syntheticKey)) {
                LaunchGiveMeLibertyFreeRoamRestoreStub("intro/GML/LM1 validated");
            } else if (ScriptRuntime::IsPlayerOnMission()) {
                // Any other synthetic mission: at least drop the stale
                // on-mission flag so the markers don't freeze.
                ScriptRuntime::SetPlayerOnMission(false);
                ScriptRuntime::FailCurrentMission() = 0;
            }
            ArmContactBlipCleanup();
            // The AP split stops EIGHT right after the hideout arrival, and the
            // vanilla "Player change" redress block sits beyond that point —
            // without this, Claude keeps the brown prison outfit forever after
            // GML. Restore the normal skin whenever the sequence completes.
            OnMissionExit(mission,
                          syntheticKey,
                          MissionExitReason::Passed,
                          "intro/GML/LM1 validated");
            if (!RunState::IsSyntheticMissionValidated(syntheticKey)) {
                RunState::ValidateSyntheticMission(syntheticKey);
                if (ScriptRuntime::IsIntroSequenceSyntheticKey(syntheticKey)) {
                    ApState::AddLocationChecked("mission_give_me_liberty");
                } else {
                    const char* loc = ScriptRuntime::SyntheticMissionLocationId(syntheticKey);
                    if (loc) {
                        ApState::AddLocationChecked(loc);
                    }
                }
                LaunchPrintNowStub("APVALID");
                Logger::Log("Mission %d passed -> validated synthetic key='%s'", mission, syntheticKey);
            }
        } else if (validationMission >= 0) {
            if (!passed) {
                Logger::Log("Mission %d ended without pass (alive=%d) -> not validated, re-arming",
                            mission, alive ? 1 : 0);
                g_onMissionScriptSeen = false;
                return;
            }
            FlushMoneyToRunState("mission validated", false);
            ArmContactBlipCleanup();
            if (!RunState::IsMissionValidated(validationMission)) {
                RunState::ValidateMission(validationMission);
                const bool showedUnlockToast = TryApplyStoryUnlockForMission(validationMission);
                if (!showedUnlockToast) {
                    LaunchPrintNowStub("APVALID");
                }
                Logger::Log("Mission %d passed -> validated index=%d", mission, validationMission);
            }
            // Emit the Archipelago location check (writes to III.Archipelago.state.json).
            const char* loc = ScriptRuntime::MissionLocationId(validationMission);
            if (loc) {
                ApState::AddLocationChecked(loc);
            }
        }
        RunState::ClearPendingValidationMission();
        RunState::SetLastLaunchedMission(-1);
        g_onMissionScriptSeen = false;
    }

#if AP_ENABLE_DEBUG_KEYS
    // Debug key (kill_npcs): walk CPools::ms_pPedPool and zero the health of
    // every live ped within ~50 m of the player (the player himself is skipped).
    // Clean — no explosion, no collateral on cars/objects/scenery.
    inline bool g_killNpcsKeyDown = false;
    inline void TryFireKillNpcsKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("kill_npcs")) & 0x8000) != 0;
        if (down && !g_killNpcsKeyDown) {
            void* playerPed = PlayerPed();
            CVector ppos{};
            if (!playerPed || !TryReadPlayerPos(&ppos)) {
                Logger::Log("Kill NPCs: no player, ignoring");
            } else {
                int killed = 0;
                __try {
                    auto pool = *reinterpret_cast<std::uint8_t**>(
                        GameAddr::Translate(GameAddr::CPools_ms_pPedPool));
                    if (pool) {
                        auto objects = *reinterpret_cast<std::uint8_t**>(pool + GameAddr::kCPool_m_pObjects);
                        auto byteMap = *reinterpret_cast<std::uint8_t**>(pool + GameAddr::kCPool_m_byteMap);
                        const int size = *reinterpret_cast<int*>(pool + GameAddr::kCPool_m_size);
                        if (objects && byteMap && size > 0 && size < 100000) {
                            constexpr float kRadiusSq = 50.0f * 50.0f;
                            for (int i = 0; i < size; ++i) {
                                if (byteMap[i] & GameAddr::kCPool_freeFlag) {
                                    continue; // free slot
                                }
                                std::uint8_t* ped = objects +
                                    static_cast<std::size_t>(i) * GameAddr::kSizeof_CPed;
                                if (ped == playerPed) {
                                    continue;
                                }
                                const float* pos = reinterpret_cast<const float*>(
                                    ped + GameAddr::CPlaceable_position);
                                const float dx = pos[0] - ppos.x;
                                const float dy = pos[1] - ppos.y;
                                const float dz = pos[2] - ppos.z;
                                if (dx * dx + dy * dy + dz * dz > kRadiusSq) {
                                    continue;
                                }
                                float* hp = reinterpret_cast<float*>(
                                    ped + GameAddr::CPed_m_fHealth);
                                if (*hp > 0.0f) {
                                    *hp = 0.0f;
                                    ++killed;
                                }
                            }
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    Logger::Log("Kill NPCs: exception 0x%08lX", GetExceptionCode());
                }
                Logger::Log("Kill NPCs: %d ped(s) killed within 50m", killed);
                LaunchPrintNowStub("APKILL");
            }
        }
        g_killNpcsKeyDown = down;
    }
#endif  // AP_ENABLE_DEBUG_KEYS

    // Archipelago weapon items: receiving one hands Claude the weapon with a
    // mission-sized ammo grant, via the same engine call the death/resume
    // restores use. GTA III eWeaponType ids (1..11, 0 = unarmed).
    struct ApWeaponItem {
        const char* name;
        int         weaponType;
        int         ammo;
    };
    inline constexpr ApWeaponItem kApWeaponItems[] = {
        {"weapon_bat",          1,   1},
        {"weapon_pistol",       2,  90},
        {"weapon_uzi",          3, 240},
        {"weapon_shotgun",      4,  40},
        {"weapon_ak47",         5, 180},
        {"weapon_m16",          6, 300},
        {"weapon_sniper",       7,  30},
        {"weapon_rocket",       8,  10},
        {"weapon_flamethrower", 9, 200},
        {"weapon_molotov",     10,   8},
        {"weapon_grenade",     11,   8},
    };

}
