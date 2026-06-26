#pragma once
// PlayerRuntime: FrameWatches (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    // Drive main.sc's unused_2 every frame from RunState::MapState. The SCM
    // mission_start polling loop reads the global on every iteration and
    // applies the matching DELETE_OBJECT + SWITCH_ROADS_ON unlock sequence
    // exactly once thanks to its flag_*_passed latches.
    inline void PushMapStateToScript() {
        // Effective island access is the highest of the local map_state (F7 /
        // checks) and what Archipelago "unlock region" items have opened.
        const int state = EffectiveMapState();
        __try {
            *reinterpret_cast<std::int32_t*>(
                ScriptRuntime::ScriptSpace() + kMapStateGlobalOffset) = state;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // ScriptSpace not mapped yet (very early boot) — fine, the next
            // frame will catch it.
        }
        // Freeze the Staunton<->Shoreside lift bridge raised until Shoreside is
        // unlocked: CStats::CommercialPassed==0 keeps CBridge in STATE_BRIDGE_LOCKED
        // (up/impassable); ==1 lets it operate once map_state reaches 2.
        __try {
            *reinterpret_cast<std::int32_t*>(
                GameAddr::Translate(GameAddr::CStats_CommercialPassed)) = (state >= 2) ? 1 : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // stats block not mapped yet — next frame will catch it.
        }
    }

    inline void ArmMissionCompletionWatch() {
        g_missionCompletionPhase = MissionCompletionPhase::Watching;
        g_missionCompletionFrames = 0;
        const SpawnPoint& home = CurrentSafehouse();
        Logger::Log("Mission completion watch armed: home=(%.1f,%.1f,%.1f) radius=%.1f",
                    home.position.x, home.position.y, home.position.z,
                    12.0f);
    }

    // CANCEL_OVERRIDE_RESTART bytecode stub: opcode 0x01F6 takes no args,
    // followed by TERMINATE_THIS_SCRIPT (0x004E). Total 4 bytes. Launch as a
    // background thread so the engine clears its CRestart override the next
    // ProcessScripts tick, before the player respawn logic kicks in. Place
    // this stub 32 bytes before SIZE_MAIN_SCRIPT, leaving the 16 bytes
    // before SIZE_MAIN_SCRIPT free for MenuPatch's MAKE_PLAYER_SAFE stub.
    constexpr std::uint32_t kCancelRestartStubOffset =
        ScmSlots::CancelRestartOffset;
    constexpr std::uint8_t kCancelRestartStub[] = {
        0xF6, 0x01,  // CANCEL_OVERRIDE_RESTART
        0x4E, 0x00,  // TERMINATE_THIS_SCRIPT
    };

    inline void LaunchCancelRestartStub() {
        CRunningScript* stub = LaunchScmStub(kCancelRestartStubOffset,
                                             kCancelRestartStub,
                                             ScmSlots::CancelRestartBytes,
                                             "cancel-restart");
        if (stub) {
            Logger::Log("Cancel-restart stub queued: ip=%u (CANCEL_OVERRIDE_RESTART)",
                        kCancelRestartStubOffset);
        }
    }

    constexpr std::uint32_t kFreeRoamRestoreStubOffset =
        ScmSlots::FreeRoamRestoreOffset;
    constexpr std::uint8_t kFreeRoamRestoreStub[] = {
        0xF6, 0x01,                               // CANCEL_OVERRIDE_RESTART
        0xB4, 0x01, 0x02, 0x10, 0x02, 0x04, 0x01, // SET_PLAYER_CONTROL &528 1
        0xA3, 0x02, 0x04, 0x00,                   // SWITCH_WIDESCREEN 0
        0xEB, 0x02,                               // RESTORE_CAMERA_JUMPCUT
        0x73, 0x03,                               // SET_CAMERA_BEHIND_PLAYER
        0xF7, 0x01, 0x02, 0x10, 0x02, 0x04, 0x00, // SET_POLICE_IGNORE_PLAYER &528 0
        0xBF, 0x03, 0x02, 0x10, 0x02, 0x04, 0x00, // SET_EVERYONE_IGNORE_PLAYER &528 0
        0x11, 0x01, 0x04, 0x01,                   // SET_DEATHARREST_STATE 1
        0x4E, 0x00                                // TERMINATE_THIS_SCRIPT
    };
    static_assert(sizeof(kFreeRoamRestoreStub) <= ScmSlots::FreeRoamRestoreBytes,
                  "free-roam restore stub no longer fits");

    inline void LaunchFreeRoamRestoreStub() {
        CRunningScript* stub = LaunchScmStub(kFreeRoamRestoreStubOffset,
                                             kFreeRoamRestoreStub,
                                             ScmSlots::FreeRoamRestoreBytes,
                                             "free-roam-restore");
        if (stub) {
            Logger::Log("Free-roam restore stub queued: ip=%u",
                        kFreeRoamRestoreStubOffset);
        }
    }

    // End the live mission and leave Claude where he is: no fade, no teleport.
    // Cancels the engine restart override first so the aborted mission's
    // deatharrest cleanup cannot respawn him elsewhere, then restores the
    // player/camera flags that a cutscene or mission vehicle flow may have left
    // disabled.
    inline void EndMissionInPlace() {
        LaunchCancelRestartStub();
        const int aborted = ScriptRuntime::ProperAbortMissionScripts();
        LaunchFreeRoamRestoreStub();
        Hooks::g_blockEngineMissionLaunches = true;

        __try {
            *reinterpret_cast<bool*>(
                GameAddr::Translate(GameAddr::CGame_playingIntro)) = false;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // ignore
        }

        ScriptRuntime::SetPlayerOnMission(false);
        ScriptRuntime::AlreadyRunningAMissionScript() = false;
        ScriptRuntime::FailCurrentMission() = 0;
        Logger::Log("End mission in place: aborted=%d script(s), free-roam restore queued",
                    aborted);
    }

    // Lift the resume black screen once the run data is genuinely live: the
    // data-storage decision arrived for this boot AND the money + loadout +
    // position restores were applied (a storage download re-arms those
    // latches, so the reveal naturally waits for the re-applied state). The
    // timeout is a safety valve for a dead bridge.
    inline void TryApplyResumeReveal();

    inline void TryFadeCamera(float seconds, short direction, const char* tag) {
        using FadeFn = void(__thiscall*)(void* self, float seconds, short direction);
        __try {
            auto fade = reinterpret_cast<FadeFn>(GameAddr::Translate(GameAddr::CCamera_Fade));
            void* theCamera = GameAddr::Translate(GameAddr::TheCamera);
            fade(theCamera, seconds, direction);
            Logger::Log("%s: CCamera::Fade(%.1fs, %s)", tag, seconds,
                        direction == 1 ? "IN" : "OUT");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("%s: CCamera::Fade raised 0x%08lX", tag, GetExceptionCode());
        }
    }

    inline void TryApplyResumeReveal() {
        if (!g_resumeRevealPending) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
            return;
        }
        ++g_resumeRevealFrames;
        const bool dataReady =
            g_storageSettledThisBoot &&
            g_moneyRestoredThisSession &&
            g_playerStateRestoredThisSession;
        const bool timedOut = g_resumeRevealFrames >= kResumeRevealTimeoutFrames;
        if (!dataReady && !timedOut) {
            return;
        }
        g_resumeRevealPending = false;
        TryFadeCamera(1.0f, 1,
                      timedOut ? "Resume reveal (timeout)" : "Resume reveal");
        Logger::Log("Resume reveal: %s after %d frames (storage=%d money=%d player=%d)",
                    timedOut ? "timeout" : "data ready",
                    g_resumeRevealFrames,
                    g_storageSettledThisBoot ? 1 : 0,
                    g_moneyRestoredThisSession ? 1 : 0,
                    g_playerStateRestoredThisSession ? 1 : 0);
    }

    // Re-assert an instant black fade without logging (called every frame while
    // the offline guard holds the screen, to override the SCM boot fade-in).
    inline void HoldCameraBlackQuiet() {
        using FadeFn = void(__thiscall*)(void* self, float seconds, short direction);
        __try {
            auto fade = reinterpret_cast<FadeFn>(GameAddr::Translate(GameAddr::CCamera_Fade));
            void* theCamera = GameAddr::Translate(GameAddr::TheCamera);
            fade(theCamera, 0.0f, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    inline void TryApplyOfflineResumeGuard() {
        // Reset per engine-life so each boot re-evaluates from scratch.
        if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
            g_offlineResumeGuard = OfflineResumeGuard::Idle;
            g_offlineResumeGuardFrames = 0;
            g_offlineResumeGuardToastShown = false;
            return;
        }
        if (g_offlineResumeGuard == OfflineResumeGuard::Done) {
            return;
        }
        // Only guards a plain free-roam RESUME (run already active). Fresh runs,
        // the INTRO and the Give Me Liberty boot own the screen themselves.
        if (!RunState::IsRunActive()) {
            return;
        }
        if (Hooks::g_patchApBootIntroLaunch ||
            RunState::IsBootIntroRequested() ||
            IsIntroSequenceLaunchPending() ||
            IsIntroSequencePendingValidation() ||
            ScriptRuntime::AlreadyRunningAMissionScript()) {
            g_offlineResumeGuard = OfflineResumeGuard::Done;
            return;
        }
        if (MenuMgr::CurrentPage() != MenuPage::None) {
            return; // wait until we're actually in-game (no frontend / pause menu)
        }

        if (ApBridge::g_connected) {
            // Connected: if we were holding black, fade the run in. If we never
            // held (bridge was already up at boot), this is a no-op online boot.
            if (g_offlineResumeGuard == OfflineResumeGuard::Holding) {
                TryFadeCamera(1.0f, 1, "Offline resume guard: bridge connected -> reveal run");
            }
            g_offlineResumeGuard = OfflineResumeGuard::Done;
            return;
        }

        // Bridge offline.
        if (g_offlineResumeGuard == OfflineResumeGuard::Idle) {
            TryFadeCamera(0.0f, 0,
                          "Offline resume guard: run active + bridge offline -> hold black");
            g_offlineResumeGuard = OfflineResumeGuard::Holding;
            g_offlineResumeGuardFrames = 0;
            g_offlineResumeGuardToastShown = false;
            return;
        }

        // Holding, still offline: keep the screen black (counter the SCM boot
        // fade-in) and run the offline timeline.
        ++g_offlineResumeGuardFrames;
        HoldCameraBlackQuiet();

        if (!g_offlineResumeGuardToastShown &&
            g_offlineResumeGuardFrames >= kOfflineGuardGraceFrames) {
            TextOverrides::SetBridgeToast("No Connected");
            LaunchPrintNowStub("APBRG", kOfflineGuardMessageMs);
            g_offlineResumeGuardToastShown = true;
            Logger::Log("Offline resume guard: bridge offline at resume -> 'No Connected' shown");
        }

        if (g_offlineResumeGuardFrames >= kOfflineGuardRevealFrames) {
            TryFadeCamera(1.0f, 1,
                          "Offline resume guard: still offline -> reveal world for free-roam");
            g_offlineResumeGuard = OfflineResumeGuard::Done;
            Logger::Log("Offline resume guard: revealed world for free-roam exploration (offline)");
        }
    }

    inline void TryApplyMissionCompletionWatch() {
        if (g_missionCompletionPhase == MissionCompletionPhase::Disarmed) {
            return;
        }

        switch (g_missionCompletionPhase) {
            case MissionCompletionPhase::Watching: {
                // Death disarms the watch for good: the wasted respawn lands at
                // the OVERRIDE_NEXT_RESTART spot — the safehouse — and without
                // this check the proximity trigger would auto-validate a
                // mission the player just FAILED by dying. The validation
                // watch handles the failed run (not passed -> re-armed).
                if (!SafeReadPlayerAlive()) {
                    Logger::Log("Mission completion watch: player died -> disarmed, no validation");
                    g_missionCompletionPhase = MissionCompletionPhase::Disarmed;
                    g_missionCompletionFrames = 0;
                    return;
                }
                CVector pos;
                if (!TryReadPlayerPos(&pos)) {
                    return;
                }
                const SpawnPoint& home = CurrentSafehouse();
                const float dx = pos.x - home.position.x;
                const float dy = pos.y - home.position.y;
                const float dz = pos.z - home.position.z;
                const float distSq = dx * dx + dy * dy + dz * dz;
                if (distSq < kMissionCompletionRadiusSq) {
                    ++g_missionCompletionFrames;
                    if (g_missionCompletionFrames >= kMissionCompletionProximityFrames) {
                        Logger::Log("Mission completion triggered: starting fade-out cinematic");
                        TryFadeCamera(1.0f, 0, "Mission completion");
                        g_missionCompletionPhase = MissionCompletionPhase::FadingOut;
                        g_missionCompletionFrames = 0;
                    }
                } else {
                    g_missionCompletionFrames = 0;
                }
                return;
            }

            case MissionCompletionPhase::FadingOut: {
                ++g_missionCompletionFrames;
                if (g_missionCompletionFrames < kMissionCompletionFadeOutFrames) {
                    return;
                }
                // Queue the engine-side restart override cancel BEFORE the
                // abort so the stub thread runs on the next tick and clears
                // CRestart::ms_bOverrideRestart before the mission cleanup's
                // RESTART_CRITICAL_MISSION setup can trigger an automatic
                // respawn at the Callahan Bridge fire spot.
                LaunchCancelRestartStub();
                const int aborted = ScriptRuntime::ProperAbortMissionScripts();
                Logger::Log("Mission completion: aborted=%d scripts during black-screen window",
                            aborted);
                Hooks::g_blockEngineMissionLaunches = true;

                const SpawnPoint& home = CurrentSafehouse();
                TryTeleportVehicleAware(home.position, &home.heading,
                                        "Mission completion", true);

                __try {
                    *reinterpret_cast<bool*>(
                        GameAddr::Translate(GameAddr::CGame_playingIntro)) = false;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // ignore
                }

                ScriptRuntime::SetPlayerOnMission(false);
                ScriptRuntime::AlreadyRunningAMissionScript() = false;
                ScriptRuntime::FailCurrentMission() = 0;

                TryFadeCamera(1.0f, 1, "Mission completion");
                g_missionCompletionPhase = MissionCompletionPhase::Restoring;
                g_missionCompletionFrames = 0;
                return;
            }

            case MissionCompletionPhase::Restoring: {
                ++g_missionCompletionFrames;
                // The deatharrest cleanup we just ran inside
                // ProperAbortMissionScripts invokes RESTART_CRITICAL_MISSION
                // on its way out. Our CANCEL_OVERRIDE_RESTART stub queued in
                // FadingOut should clear that, but the engine may still
                // respawn Claude on the bridge if the timing works against
                // us. Re-teleport ONLY when Claude has actually drifted from
                // the safehouse — this avoids hammering CPed::Teleport every
                // frame, which was spamming the zone subsystem and tripping
                // the CTheZones crash at 0x004B671A on the rapid Industrial
                // ↔ Callahan-bridge zone transitions.
                CVector pos{};
                if (TryReadPlayerPos(&pos)) {
                    const SpawnPoint& home = CurrentSafehouse();
                    const float dx = pos.x - home.position.x;
                    const float dy = pos.y - home.position.y;
                    const float dz = pos.z - home.position.z;
                    const float distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq > 6.0f * 6.0f) {
                        if (TryTeleportVehicleAware(home.position, &home.heading,
                                                    "Mission completion", true)) {
                            Logger::Log("Mission completion: drift detected, re-teleport home (frame=%d, dist=%.1f)",
                                        g_missionCompletionFrames,
                                        std::sqrt(distSq));
                        }
                    }
                }
                if (g_missionCompletionFrames >= kMissionCompletionFadeInFrames) {
                    Logger::Log("Mission completion: disarmed, player restored to safehouse");
                    // The launched mission is now done: validate it (V marker)
                    // and pop a toast, once.
                    const int done = RunState::LastLaunchedMission();
                    if (done >= 0 && !RunState::IsMissionValidated(done)) {
                        RunState::ValidateMission(done);
                        const char* loc = ScriptRuntime::MissionLocationId(done);
                        if (loc) {
                            ApState::AddLocationChecked(loc);
                        }
                        const bool showedUnlockToast = TryApplyStoryUnlockForMission(done);
                        if (!showedUnlockToast) {
                            LaunchPrintNowStub("APVALID");
                        }
                        Logger::Log("Mission completion: auto-validated mission index=%d", done);
                    }
                    if (done >= 0) {
                        ArmContactBlipCleanup();
                    }
                    RunState::SetLastLaunchedMission(-1);
                    g_missionCompletionPhase = MissionCompletionPhase::Disarmed;
                    g_missionCompletionFrames = 0;
                }
                return;
            }

            default:
                return;
        }
    }
}
