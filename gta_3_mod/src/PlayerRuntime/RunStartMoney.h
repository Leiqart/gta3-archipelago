#pragma once
// PlayerRuntime: RunStartMoney (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    inline void CancelPendingSafehouseWarp() {
        if (!g_pendingSafehouseWarp) {
            return;
        }
        g_pendingSafehouseWarp = false;
        g_safehouseWarpFrames = 0;
        g_fastSafehouseWarp = false;
        g_safehouseWarpOutfitApplied = false;
        g_runStartOutfitPreloadApplied = false;
        Logger::Log("Pending safehouse warp canceled (boot-intro owns the boot)");
    }

    inline bool ConfigureMissionLaunch(const ScriptRuntime::VisibleMissionEntry& entry) {
        return OnMissionEnter(entry,
                              static_cast<std::uint32_t>(
                                  ScriptRuntime::kSizeMainScript),
                              "configured launch");
    }

    // Synchronously teleport Claude to a mission's trigger location, called
    // by LaunchMission right before StartNewScript. Vanilla *_mission_loop
    // logic guarantees Claude is standing on the marker when the mission
    // bytecode boots; the pause-menu launch path has to fake that on its
    // own or the mission opens with the camera looking at an empty scene.
    inline bool TryTeleportToMissionSpawn(
        int actualIndex,
        ScriptRuntime::MissionLaunchVariant variant = ScriptRuntime::MissionLaunchVariant::Standard) {
        const MissionSpawn* entry = FindMissionSpawn(actualIndex, variant);
        if (!entry) {
            return false;
        }

        void* ped = PlayerPed();
        if (!ped) {
            Logger::Log("Mission spawn teleport: PlayerPed null for index=%d",
                        actualIndex);
            return false;
        }

        __try {
            auto teleport = reinterpret_cast<TeleportFn>(
                GameAddr::Translate(GameAddr::CPed_Teleport));
            teleport(ped, entry->spawn.position);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Mission spawn teleport raised exception 0x%08lX for index=%d",
                        GetExceptionCode(), actualIndex);
            return false;
        }

        Logger::Log("Mission spawn teleport: index=%d variant=%d x=%.1f y=%.1f z=%.1f",
                    actualIndex, static_cast<int>(variant),
                    entry->spawn.position.x,
                    entry->spawn.position.y,
                    entry->spawn.position.z);
        return true;
    }

    inline void QueueSafehouseWarp(bool fast = false) {
        g_pendingSafehouseWarp = true;
        g_safehouseWarpFrames = 0;
        g_fastSafehouseWarp = fast;
        g_safehouseWarpOutfitApplied = false;
        g_runStartOutfitPreloadApplied = false;
        QueuePlayerOutfitRestore("safehouse warp queued");

        const SpawnPoint& spawn = CurrentSafehouse();
        Logger::Log("Queued safehouse warp: checks=%d fast=%d x=%.1f y=%.1f z=%.1f heading=%.1f",
                    RunState::Checks(),
                    g_fastSafehouseWarp ? 1 : 0,
                    spawn.position.x,
                    spawn.position.y,
                    spawn.position.z,
                    spawn.heading);
    }

    inline bool HasPendingSafehouseWarp() {
        return g_pendingSafehouseWarp;
    }

    inline void QueueStartupApStateReset() {
        g_pendingStartupApStateReset = true;
        Logger::Log("Queued startup Archipelago state reset");
    }

    inline void TryApplyRunStartOutfitPreload() {
        const bool wanted =
            (g_pendingSafehouseWarp || RunState::IsRunStartPending()) &&
            !Hooks::g_patchApBootIntroLaunch;
        if (!wanted) {
            g_runStartOutfitPreloadApplied = false;
            return;
        }
        if (g_runStartOutfitPreloadApplied) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            g_runStartOutfitPreloadApplied = false;
            return;
        }
        if (!PlayerPed()) {
            return;
        }

        if (LaunchApplyPlayerSkinStub("PLAYERX", true, "run start preload")) {
            g_runStartOutfitPreloadApplied = true;
            g_safehouseWarpOutfitApplied = true;
            g_outfitRestorePending = false;
            Logger::Log("Run start pre-applied PLAYERX outfit");
        }
    }

    inline bool TryApplyPendingSafehouseWarp() {
        if (!g_pendingSafehouseWarp) {
            return false;
        }

        // A vanilla-style intro boot is armed: the engine restart will consume
        // it and cancel this warp. Hold the warp meanwhile so it cannot
        // complete in the dying pre-restart world and disarm the intro boot by
        // mistake. If no restart shows up within ~30 s, the run start happened
        // in the live world after all — disarm and let the warp take over.
        if (Hooks::g_patchApBootIntroLaunch) {
            if (++g_bootIntroWaitFrames < 1800) {
                return false;
            }
            DisarmBootIntroSequence("no engine restart within ~30 s of run start");
        }

        if (!ScriptRuntime::HasLiveScriptEngine()) {
            g_safehouseWarpOutfitApplied = false;
            g_runStartOutfitPreloadApplied = false;
            return false;
        }

        void* ped = PlayerPed();
        if (!ped) {
            return false;
        }

        if (!g_safehouseWarpOutfitApplied) {
            if (LaunchApplyPlayerSkinStub("PLAYERX", true,
                                          "safehouse warp pre-stabilize")) {
                g_safehouseWarpOutfitApplied = true;
                g_outfitRestorePending = false;
                Logger::Log("Safehouse warp pre-applied PLAYERX outfit");
            }
        }

        const int stabilizeFrames = g_fastSafehouseWarp
            ? kFastWarpStabilizeFrames
            : kWarpStabilizeFrames;

        if (g_safehouseWarpFrames < stabilizeFrames) {
            ++g_safehouseWarpFrames;
            if (g_safehouseWarpFrames == 1) {
                Logger::Log("Safehouse warp waiting for gameplay to stabilise (fast=%d)",
                            g_fastSafehouseWarp ? 1 : 0);
            }
            // Keep poking the active INTRO cutscene-skip flag every frame while we wait for
            // gameplay to stabilise. INTRO resets it to 0 once in its
            // init (main.scm:12082), so a one-shot write at script
            // launch gets clobbered — but eventually our write lands
            // AFTER the reset and the cutscene loop's check on skip_flag==2
            // jumps to the post-skip cleanup (MISSION_2_176).
            __try {
                *reinterpret_cast<std::int32_t*>(
                    ScriptRuntime::ScriptSpace() + kIntroSkipFlagOffset) = 2;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // ScriptSpace not mapped yet, ignore.
            }
            return false;
        }

        // Resume lands directly on the saved position when there is one — the
        // old flow warped to the safehouse and the persistence restore yanked
        // the player to the saved spot a second later ("tp bizarre").
        const SpawnPoint& spawn = CurrentSafehouse();
        CVector warpTarget = spawn.position;
        bool usingSavedPosition = false;
        {
            float sx = 0.0f, sy = 0.0f, sz = 0.0f;
            if (RunState::SavedPosition(&sx, &sy, &sz)) {
                warpTarget = {sx, sy, sz};
                usingSavedPosition = true;
            }
        }
        __try {
            auto teleport = reinterpret_cast<TeleportFn>(GameAddr::Translate(GameAddr::CPed_Teleport));
            teleport(ped, warpTarget);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Safehouse warp raised exception 0x%08lX", GetExceptionCode());
            g_pendingSafehouseWarp = false;
            g_safehouseWarpFrames = 0;
            g_fastSafehouseWarp = false;
            g_safehouseWarpOutfitApplied = false;
            g_runStartOutfitPreloadApplied = false;
            RunState::SetRunStartPending(false);
            RunState::SetSkipIntroOnNextNewGame(false);
            return false;
        }

        g_pendingSafehouseWarp = false;
        g_safehouseWarpFrames = 0;
        g_fastSafehouseWarp = false;
        g_safehouseWarpOutfitApplied = false;
        g_runStartOutfitPreloadApplied = false;
        RunState::SetRunStartPending(false);
        RunState::SetSkipIntroOnNextNewGame(false);

        if (usingSavedPosition) {
            // Arbitrary resume spot: stream the area in synchronously so the
            // player does not stand over unloaded map (the safehouse target
            // never needed this — the boot scene covers it).
            TryLoadSceneAt(warpTarget, "Resume warp");
            TryLoadAllRequestedModels("Resume warp");
            Logger::Log("Resume warp landed on saved position (%.1f,%.1f,%.1f)",
                        warpTarget.x, warpTarget.y, warpTarget.z);
        }

        // The warp completing means the run start happened in the live world —
        // the engine never reloaded the SCM, so a still-armed boot-intro would
        // never fire and would block the mission selector forever.
        if (Hooks::g_patchApBootIntroLaunch) {
            DisarmBootIntroSequence("run started in live world, no engine restart");
        }

        // Freeroam always means civilian Claude: if a previous session left the
        // prison outfit on (intro/GML interrupted before any of the redress
        // hooks existed), this repairs it once the spawn settles.
        QueuePlayerOutfitRestore("safehouse warp completed");

        // Take the engine fully out of "intro is playing" mode. The script
        // normally does SET_INTRO_IS_PLAYING 0 once the cutscene ends;
        // we do it ourselves to make sure the engine drops the loading
        // overlay even if INTRO terminated via auto-skip instead of the
        // full cutscene run.
        __try {
            *reinterpret_cast<bool*>(GameAddr::Translate(GameAddr::CGame_playingIntro)) = false;
            Logger::Log("Cleared CGame::playingIntro after safehouse warp");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Writing CGame::playingIntro raised exception 0x%08lX",
                        GetExceptionCode());
        }

        // Now that the warp has placed Claude and the screen is fading
        // back in, arm the engine-mission block so the next auto-launch
        // (LM1 / EIGHT when Claude is near Luigi's club, RC challenges,
        // ambient missions) gets TERMINATE_THIS_SCRIPT patched into its
        // first opcode. INTRO is already running by this point — if it
        // hasn't ended via auto-skip yet, the block kicks in for the
        // mission AFTER INTRO; INTRO itself isn't disturbed.
        Hooks::g_blockEngineMissionLaunches = true;

        // The screen stays BLACK from here: instead of fading in right away
        // (which exposed the spawn before the data-storage download, the
        // money restore and the position restore landed — "spawn, TP, money
        // pops"), hold the fade and let TryApplyResumeReveal lift it once the
        // run data is live (or its safety timeout fires).
        using FadeFn = void(__thiscall*)(void* self, float seconds, short direction);
        __try {
            auto fade = reinterpret_cast<FadeFn>(GameAddr::Translate(GameAddr::CCamera_Fade));
            void* theCamera = GameAddr::Translate(GameAddr::TheCamera);
            fade(theCamera, 0.0f, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("CCamera::Fade(hold) raised exception 0x%08lX after safehouse warp",
                        GetExceptionCode());
        }
        g_resumeRevealPending = true;
        g_resumeRevealFrames = 0;
        Logger::Log("Resume reveal armed: holding black until run data is loaded");

        Logger::Log("Applied safehouse warp: checks=%d x=%.1f y=%.1f z=%.1f heading=%.1f",
                    RunState::Checks(),
                    spawn.position.x,
                    spawn.position.y,
                    spawn.position.z,
                    spawn.heading);
        return true;
    }

    inline void TryApplyStartupApStateReset() {
        if (!g_pendingStartupApStateReset) {
            return;
        }
        if (g_pendingSafehouseWarp) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }

        ApState::ResetForNewRun();
        g_pendingStartupApStateReset = false;
        Logger::Log("Applied startup Archipelago state reset");
    }

    // Re-arm the money restore for a fresh run started in the same process. The
    // restore latch is otherwise sticky for the whole session; a new run resets
    // RunState::Money() to 0 and must force the live cash back to 0 rather than
    // letting the sync persist the previous run's leftover cash.
    inline void ResetMoneyPersistenceSession() {
        g_moneyRestoredThisSession = false;
        g_lastSyncedMoney = -1;
        g_moneySyncFrames = 0;
    }

    // Player loadout/vitals/position/packages persistence (same lifecycle as the
    // money latch above): restore the RunState snapshot once per spawned
    // session, then mirror the live ped back into RunState on a slow tick.
    // Re-armed by new runs and by Archipelago data-storage downloads so a
    // server-side state lands on the live ped, not just on disk.
    inline bool g_playerStateRestoredThisSession = false;
    inline int  g_playerSyncFrames = 0;
    constexpr int kPlayerSyncIntervalFrames = 90;  // ~1.5 s @ 60 fps

    inline void ResetPlayerPersistenceSession() {
        g_playerStateRestoredThisSession = false;
        g_playerSyncFrames = 0;
    }

    // True while the intro -> Give Me Liberty sequence still owes its
    // validation, i.e. the cinematic or GML is running (or about to). The
    // money/loadout restores and item grants must not fire into that
    // sequence — a vanilla-style boot plays it before freeroam exists.
    inline bool IsIntroSequencePendingValidation() {
        return ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(
            RunState::PendingValidationSyntheticMission());
    }

    inline bool IsAnyMissionPendingValidation() {
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        return RunState::PendingValidationMission() >= 0 ||
               (syntheticKey && syntheticKey[0] != '\0');
    }

    inline bool IsMissionRuntimeBusy() {
        return IsAnyMissionPendingValidation() ||
               ScriptRuntime::AlreadyRunningAMissionScript() ||
               ScriptRuntime::IsPlayerOnMission();
    }

    inline bool FlushMoneyToRunState(const char* reason, bool requireRestoreLatch = true) {
        if (!RunState::IsRunLive()) {  // offline: never mirror vanilla cash over the run's saved money
            return false;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return false;
        }
        if (g_pendingSafehouseWarp || RunState::IsRunStartPending()) {
            return false;
        }
        if (IsIntroSequencePendingValidation()) {
            return false;
        }
        if (!PlayerPed()) {
            return false;
        }
        // Never mirror pre-restore cash: until the saved total has landed in the
        // live game, the engine value may still be boot/default teardown state.
        if (requireRestoreLatch && !g_moneyRestoredThisSession) {
            return false;
        }

        int live = 0;
        if (!GameAddr::TryReadPlayerCash(&live)) {
            return false;
        }
        if (live == g_lastSyncedMoney) {
            return false;
        }

        RunState::SetMoney(live);
        g_lastSyncedMoney = live;
        g_moneySyncFrames = 0;
        Logger::Log("Money flushed to RunState (%s): $%d",
                    reason ? reason : "unspecified", live);
        return true;
    }

    // Give Me Liberty already validated for this run? Drives the boot-mode
    // choice: not yet -> vanilla-style intro boot; done -> freeroam boot.
    // Reads the local files directly (the session trust latches are not armed
    // this early); a stale local state at worst replays a skippable intro.
    inline bool IsGiveMeLibertyCompleted() {
        return RunState::IsSyntheticMissionValidated("APINTRO") ||
               RunState::IsSyntheticMissionValidated("APGMLIB") ||
               ApState::IsLocationChecked("mission_give_me_liberty");
    }

    // Restore the persisted money total over the spawn cash once per session,
    // then mirror the live cash into RunState so it persists across reconnects.
    inline void TryApplyMoneyPersistence() {
        if (!RunState::IsRunLive()) {  // offline = no run: don't restore run money
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        // Don't touch cash until the player has actually spawned into the world:
        // a pending warp or run-start means the boot SCM is still settling and
        // the cash address would read/overwrite a transient value.
        if (g_pendingSafehouseWarp || RunState::IsRunStartPending()) {
            return;
        }
        if (IsMissionRuntimeBusy()) {
            return;
        }
        // A vanilla-style boot plays the intro/GML before freeroam exists;
        // hold the restore until that sequence validated.
        if (IsIntroSequencePendingValidation()) {
            return;
        }
        // Menu open (incl. the quit-to-frontend teardown): cash cannot change
        // legitimately, but the engine zeroes it while unloading the session —
        // a mirror tick there persisted $0 over the real total. Freeze both
        // directions until pure gameplay.
        if (MenuMgr::CurrentPage() != MenuPage::None) {
            return;
        }
        if (!PlayerPed()) {
            return;
        }

        if (!g_moneyRestoredThisSession) {
            const int saved = RunState::Money();
            if (!GameAddr::TryWritePlayerCash(saved)) {
                return;  // address not live yet; retry next frame
            }
            g_moneyRestoredThisSession = true;
            g_lastSyncedMoney = saved;
            g_moneySyncFrames = 0;
            Logger::Log("Money restored on resume: $%d", saved);
            return;
        }

        if (++g_moneySyncFrames < kMoneySyncIntervalFrames) {
            return;
        }
        g_moneySyncFrames = 0;

        FlushMoneyToRunState("periodic");
    }

    // ----- mission completion fallback --------------------------------------
    //
    // The vanilla mission scripts expect a chain of trigger loops, flags and
    // cutscenes that we sidestep by launching from the pause menu. EIGHT's
    // drop-off marker is the worst symptom: Claude can drive 8-Ball back to
    // the safehouse but the marker never fires its completion sequence, so
    // the player is stuck on a blue marker with no objective.
    //
    // This watcher arms at LaunchMission time and polls the player's
    // position every CMenuManager::Process tick (which the engine calls
    // once per frame even outside the menu). The first time Claude lingers
    // inside a small radius around his current safehouse for ~30 frames,
    // we run a "mission cleanup" sequence:
    //   1. CCamera::Fade(1.0s, OUT) — black out
    //   2. wait ~60 frames for the fade to land
    //   3. ProperAbortMissionScripts — terminate the live mission cleanly
    //   4. CPed::Teleport — put Claude in front of the safehouse
    //   5. CCamera::Fade(1.0s, IN) — fade back in
    //   6. disarm
    enum class MissionCompletionPhase : std::uint8_t {
        Disarmed,
        Watching,
        FadingOut,
        Restoring,
    };

    inline MissionCompletionPhase g_missionCompletionPhase = MissionCompletionPhase::Disarmed;
    inline int g_missionCompletionFrames = 0;
    constexpr int   kMissionCompletionProximityFrames = 30;
    constexpr int   kMissionCompletionFadeOutFrames   = 60;
    constexpr int   kMissionCompletionFadeInFrames    = 60;
    constexpr float kMissionCompletionRadiusSq        = 12.0f * 12.0f;
    constexpr std::uint16_t kContactBlipGlobalOffsets[] = {
        844, 848, 852, 856, 860, 864, 868,
        872, 876, 880, 884, 888, 892,
    };

    // Offset of main.sc's `unused_2` global inside ScriptSpace, repurposed as
    // the AP map progression value. Lifted from the compiled main_ap.scm
    // decompile where SET_VAR_INT &144 mirrors the `unused_2 = N` assignments
    // at the end of the ARCHIPELAGO_BOOT block. Any time we add a VAR_INT
    // declaration before unused_2 in main.sc, this needs to be revisited.
    constexpr std::uint32_t kMapStateGlobalOffset = 144;

    // PRINT_NOW 'TEST_AP' 3000 1 + TERMINATE_THIS_SCRIPT bytecode stub.
    // GTA3 PRINT_NOW is opcode 0x00BC (0x00BA is PRINT_BIG — the big centered
    // mission-title text — which is what the previous build was wrongly
    // triggering). The opcode encodes its GXT key as 8 raw bytes immediately
    // after the opcode (no type prefix), followed by the duration (int16)
    // and flag (int8) with the normal type-prefix encoding. The engine
    // appends the message to the brief queue automatically when type=1,
    // so the same "test" string lands in the pause-menu Brief page.
    //
    // Bytes:
    //   BC 00                       PRINT_NOW
    //   54 45 53 54 5F 41 50 00     'TEST_AP\0' (8 bytes, raw)
    //   05 B8 0B                    INT16 type + 3000 ms LE
    //   04 01                       INT8 type + flag 1
    //   4E 00                       TERMINATE_THIS_SCRIPT
    constexpr std::uint32_t kTestPrintStubOffset =
        ScmSlots::PrintNowOffset;
    constexpr std::uint8_t kTestPrintStub[] = {
        0xBC, 0x00,
        'T',  'E',  'S',  'T',  '_',  'A',  'P',  0x00,
        0x05, 0xB8, 0x0B,
        0x04, 0x01,
        0x4E, 0x00,
    };
    static_assert(sizeof(kTestPrintStub) == 17,
                  "PRINT_NOW test stub size mismatch");

}
