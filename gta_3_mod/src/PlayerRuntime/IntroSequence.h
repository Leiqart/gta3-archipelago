#pragma once
// PlayerRuntime: IntroSequence (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    inline void ArmIntroBankSceneWatch() {
        g_introBankSceneFramesLeft = kIntroBankSceneWatchFrames;
        Logger::Log("Armed INTRO bank scene watch: %d frames stride=%d",
                    kIntroBankSceneWatchFrames, kIntroBankSceneWatchStride);
    }

    inline void TryApplyIntroBankSceneWatch() {
        if (g_introBankSceneFramesLeft <= 0) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        --g_introBankSceneFramesLeft;
        if (g_introBankSceneFramesLeft == 0 ||
            (g_introBankSceneFramesLeft % kIntroBankSceneWatchStride) != 0) {
            return;
        }
        TryLoadAllRequestedModels("INTRO bank request pump");
    }

    inline bool FireManualIntroLaunchTrigger() {
        __try {
            *reinterpret_cast<std::int32_t*>(
                ScriptRuntime::ScriptSpace() + kLuigiHelpMessageGlobalOffset) =
                kLaunchIntroFromPauseValue;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Manual INTRO delayed trigger failed: exception 0x%08lX",
                        GetExceptionCode());
            return false;
        }
        Logger::Log("Manual INTRO delayed trigger fired (&36=%d)",
                    static_cast<int>(kLaunchIntroFromPauseValue));
        return true;
    }

    inline void ArmManualIntroLaunchTrigger() {
        g_manualIntroTriggerFrames = kManualIntroTriggerDelayFrames;
        Logger::Log("Armed manual INTRO delayed trigger: %d frames",
                    kManualIntroTriggerDelayFrames);
    }

    inline void TryApplyManualIntroLaunchTrigger() {
        if (g_manualIntroTriggerFrames <= 0) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        --g_manualIntroTriggerFrames;
        if (g_manualIntroTriggerFrames > 0) {
            return;
        }
        Hooks::g_allowNextEngineIntroLaunch = true;
        if (!FireManualIntroLaunchTrigger()) {
            Hooks::g_allowNextEngineIntroLaunch = false;
        }
    }

    inline void ArmIntroTeleportWatch() {
        g_introTeleportArmed = true;
        g_introTeleportFramesLeft = kIntroTeleportWatchFrames;
        Logger::Log("Armed INTRO teleport watch: waiting for Claude at (%.1f,%.1f,%.1f) within %d frames",
                    kGiveMeLibertyScene.x, kGiveMeLibertyScene.y, kGiveMeLibertyScene.z,
                    kIntroTeleportWatchFrames);
    }

    inline void TryApplyIntroTeleportWatch() {
        if (!g_introTeleportArmed) {
            return;
        }
        if (g_introTeleportFramesLeft <= 0) {
            Logger::Log("INTRO teleport watch expired without trigger");
            g_introTeleportArmed = false;
            return;
        }
        --g_introTeleportFramesLeft;

        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }

        CVector pos{};
        if (!TryReadPlayerPos(&pos)) {
            return;
        }

        const float dx = pos.x - kGiveMeLibertyScene.x;
        const float dy = pos.y - kGiveMeLibertyScene.y;
        const float dz = pos.z - kGiveMeLibertyScene.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > kIntroTeleportRadiusSq) {
            return;
        }

        Logger::Log("INTRO teleport detected: Claude at (%.1f,%.1f,%.1f) dist=%.1f",
                    pos.x, pos.y, pos.z, std::sqrt(distSq));
        g_introBankSceneFramesLeft = 0;
        g_introTeleportArmed = false;
    }

    inline void QueuePostLaunchStreamFlush() {
        g_streamFlushCountdown = kStreamFlushDelayFrames + kStreamFlushDurationFrames;
        g_streamFlushApplied   = 0;
        Logger::Log("Queued post-launch streaming flush: delay=%d duration=%d",
                    kStreamFlushDelayFrames, kStreamFlushDurationFrames);
    }

    inline void TryApplyPostLaunchStreamFlush() {
        if (g_streamFlushCountdown <= 0) {
            return;
        }
        --g_streamFlushCountdown;

        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }

        // Hold off for the first few frames so the mission script gets a
        // chance to teleport Claude into the mission's start area. After
        // that fire a LoadScene every few frames so streaming catches up
        // through the whole intro/cutscene window.
        if (g_streamFlushCountdown > kStreamFlushDurationFrames) {
            return;
        }
        if ((g_streamFlushCountdown % kStreamFlushStrideFrames) != 0) {
            return;
        }

        CVector pos{};
        if (!TryReadPlayerPos(&pos)) {
            return;
        }

        auto loadScene = reinterpret_cast<LoadSceneFn>(
            GameAddr::Translate(GameAddr::CStreaming_LoadScene));
        __try {
            loadScene(&pos);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("LoadScene raised exception 0x%08lX at (%.1f,%.1f,%.1f); aborting flush",
                        GetExceptionCode(), pos.x, pos.y, pos.z);
            g_streamFlushCountdown = 0;
            return;
        }

        ++g_streamFlushApplied;
        Logger::Log("Post-launch LoadScene #%d at (%.1f,%.1f,%.1f) — %d frames left",
                    g_streamFlushApplied, pos.x, pos.y, pos.z, g_streamFlushCountdown);
    }

    inline void ArmIntroSkipPump() {
        g_introSkipPumpFrames = kIntroSkipPumpFrames;
        Logger::Log("Armed INTRO skip pump for %d frames (~%ds) offset=%d",
                    kIntroSkipPumpFrames,
                    kIntroSkipPumpFrames / 60,
                    static_cast<int>(kIntroSkipFlagOffset));
    }

    inline void TryPumpIntroSkipFlag() {
        if (g_introSkipPumpFrames <= 0) {
            return;
        }
        --g_introSkipPumpFrames;
        __try {
            *reinterpret_cast<std::int32_t*>(
                ScriptRuntime::ScriptSpace() + kIntroSkipFlagOffset) = 2;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // ScriptSpace not mapped yet.
        }
    }

    // main.sc declares unused_1 before mission_trigger_wait_time, but the
    // compiled script reserves the first two 4-byte globals for engine-owned
    // state. We can anchor the layout from the decompile:
    //   mission_trigger_wait_time = &20
    //   flag_reached_hideout      = &32
    // Therefore:
    //   unused_1  = &8
    //   joeys_buggy = &12
    //   swank_taxi  = &16
    // The split EIGHT script reads unused_1 to decide whether it should stop
    // after Give Me Liberty or jump directly to Luigi's Girls.
    constexpr std::uint32_t kEightSplitModeGlobalOffset = 8;
    inline bool TryReadEightSplitMode(float* out) {
        if (!out || !ScriptRuntime::HasLiveScriptEngine()) {
            return false;
        }
        __try {
            *out = *reinterpret_cast<float*>(
                ScriptRuntime::ScriptSpace() + kEightSplitModeGlobalOffset);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool IsLuigisGirlsDirectMode() {
        float splitMode = 0.0f;
        return TryReadEightSplitMode(&splitMode) &&
               splitMode > 1.5f && splitMode < 2.5f;
    }

    inline bool IsGiveMeLibertyStartMode() {
        return !IsLuigisGirlsDirectMode();
    }

    inline std::size_t BuildGiveMeLibertyFreeRoamRestoreBytecode(std::uint8_t* out,
                                                                 std::size_t cap,
                                                                 const CVector* exitPos) {
        if (!out || cap < 48) {
            return 0;
        }

        std::size_t n = 0;
        auto emit = [&](std::uint8_t b) {
            out[n++] = b;
        };
        auto emitBytes = [&](const std::uint8_t* bytes, std::size_t count) {
            std::memcpy(out + n, bytes, count);
            n += count;
        };
        auto emitPlayerHandle = [&]() {
            const std::uint8_t player[] = {0x02, 0x10, 0x02}; // &528
            emitBytes(player, sizeof(player));
        };
        auto emitInt8 = [&](std::uint8_t value) {
            emit(0x04);
            emit(value);
        };
        auto emitScmFloat = [&](float value) {
            long encoded = std::lround(value * 16.0f);
            if (encoded > 32767) { encoded = 32767; }
            if (encoded < -32768) { encoded = -32768; }
            emit(0x06);
            emit(static_cast<std::uint8_t>(encoded & 0xFF));
            emit(static_cast<std::uint8_t>((encoded >> 8) & 0xFF));
        };

        if (exitPos) {
            emit(0x2A); emit(0x01);                     // WARP_PLAYER_FROM_CAR_TO_COORD
            emitPlayerHandle();
            emitScmFloat(exitPos->x);
            emitScmFloat(exitPos->y);
            emitScmFloat(exitPos->z);
        }
        emit(0xB4); emit(0x01);                         // SET_PLAYER_CONTROL
        emitPlayerHandle();
        emitInt8(1);
        emit(0xA3); emit(0x02); emitInt8(0);            // SWITCH_WIDESCREEN 0
        emit(0xEB); emit(0x02);                         // RESTORE_CAMERA_JUMPCUT
        emit(0x73); emit(0x03);                         // SET_CAMERA_BEHIND_PLAYER
        emit(0xF7); emit(0x01);                         // SET_POLICE_IGNORE_PLAYER &528 0
        emitPlayerHandle();
        emitInt8(0);
        emit(0xBF); emit(0x03);                         // SET_EVERYONE_IGNORE_PLAYER &528 0
        emitPlayerHandle();
        emitInt8(0);
        emit(0xF6); emit(0x01);                         // CANCEL_OVERRIDE_RESTART
        emit(0x11); emit(0x01); emitInt8(1);            // SET_DEATHARREST_STATE 1
        emit(0x4E); emit(0x00);                         // TERMINATE_THIS_SCRIPT
        return n;
    }

    inline void LaunchGiveMeLibertyFreeRoamRestoreStub(const char* reason) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }

        GameAddr::TryWritePlayerWbState(GameAddr::kWbStatePlaying);
        ScriptRuntime::SetPlayerOnMission(false);
        ScriptRuntime::FailCurrentMission() = 0;

        CVector exitPos{};
        const bool wasInVehicle = PlayerVehicle() != nullptr;
        const bool hasExitPos =
            wasInVehicle &&
            (TryReadVehiclePos(&exitPos) || TryReadPlayerPos(&exitPos));

        std::uint8_t stub[ScmSlots::GmlFreeRoamRestoreBytes] = {};
        const std::size_t n =
            BuildGiveMeLibertyFreeRoamRestoreBytecode(
                stub,
                sizeof(stub),
                hasExitPos ? &exitPos : nullptr);
        CRunningScript* s = LaunchScmStub(ScmSlots::GmlFreeRoamRestoreOffset,
                                          stub,
                                          n,
                                          ScmSlots::GmlFreeRoamRestoreBytes,
                                          "GML free-roam restore");
        if (s) {
            if (hasExitPos) {
                Logger::Log("GML free-roam restore queued: ip=%u bytes=%zu current-pos=(%.1f,%.1f,%.1f) reason=%s",
                            ScmSlots::GmlFreeRoamRestoreOffset,
                            n,
                            exitPos.x, exitPos.y, exitPos.z,
                            reason ? reason : "unspecified");
            } else {
                Logger::Log("GML free-roam restore queued: ip=%u bytes=%zu no-warp vehicle=%d reason=%s",
                            ScmSlots::GmlFreeRoamRestoreOffset,
                            n,
                            wasInVehicle ? 1 : 0,
                            reason ? reason : "unspecified");
            }
        }
    }

    inline bool PatchGiveMeLibertyLm1Boundary(std::uint32_t scriptIp,
                                              const char* reason) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return false;
        }

        // EIGHT reaches this exact sequence after the whole Give Me Liberty
        // hideout handoff, immediately before Luigi's Girls starts:
        // MAKE_PLAYER_SAFE_FOR_CUTSCENE, fade/streaming setup, PRINT_BIG 'LM1'.
        constexpr std::uint8_t kLm1BoundaryPattern[] = {
            0xEF, 0x03, 0x02, 0x10, 0x02,             // MAKE_PLAYER_SAFE_FOR_CUTSCENE &528
            0x69, 0x01, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00,
            0x6A, 0x01, 0x05, 0xDC, 0x05, 0x04, 0x00,
            0xAF, 0x03, 0x04, 0x00,
            0xBA, 0x00, 'L', 'M', '1', 0x00
        };

        // GML normally continues into LM1 from inside the car handoff flow.
        // Since AP cuts the chain here, terminate the chain and restore
        // control/camera flags without a fixed-position teleport. The deferred
        // OnMissionExit stub handles the optional car exit at the current spot.
        std::uint8_t splitExitPatch[ScmSlots::GmlFreeRoamRestoreBytes] = {};
        const std::size_t patchSize =
            BuildGiveMeLibertyFreeRoamRestoreBytecode(splitExitPatch,
                                                      sizeof(splitExitPatch),
                                                      nullptr);
        if (patchSize == 0) {
            return false;
        }

        constexpr std::uint32_t kScriptSpaceBytes = 0x80000;
        constexpr std::uint32_t kSearchBytes = 32768;
        if (scriptIp >= kScriptSpaceBytes) {
            return false;
        }

        std::uint8_t* scriptSpace = ScriptRuntime::ScriptSpace();
        const std::uint32_t searchEnd =
            std::min<std::uint32_t>(kScriptSpaceBytes,
                                    scriptIp + kSearchBytes);
        for (std::uint32_t ip = scriptIp;
              ip + sizeof(kLm1BoundaryPattern) <= searchEnd &&
              ip + patchSize <= kScriptSpaceBytes;
              ++ip) {
            if (std::memcmp(scriptSpace + ip,
                            kLm1BoundaryPattern,
                            sizeof(kLm1BoundaryPattern)) != 0) {
                continue;
            }
            std::memcpy(scriptSpace + ip, splitExitPatch, patchSize);
            Logger::Log("GML split exit patched before LM1: ip=%u bytes=%zu reason=%s",
                        ip, patchSize, reason ? reason : "unspecified");
            return true;
        }

        Logger::Log("GML split exit patch not found from ip=%u reason=%s",
                    scriptIp, reason ? reason : "unspecified");
        return false;
    }

    // main.sc line 33: flag_reached_hideout. EIGHT writes this to 1 once the
    // safehouse segment completed; if a later selector launch inherits that
    // stale value, the mission skips the bridge/prisoner intro and jumps
    // straight into the car-boarding restart path with uninitialised car data.
    constexpr std::uint32_t kEightReachedHideoutGlobalOffset = 32;
    // main.sc line 417: flag_luigis_girls_started. EIGHT sets this after the
    // LM1 cutscene and before Misty gameplay, which is the reliable boundary
    // where GML fail-fast must stop intercepting critical restarts.
    constexpr std::uint32_t kLuigisGirlsStartedGlobalOffset = 992;
    constexpr int kIntroGmlSplitGuardDelayFrames = 30;
    inline int g_introGmlSplitGuardFrames = 0;
    inline bool g_introEightballStarted = false;
    constexpr int kIntroFollowupLaunchDelayFrames = 45;
    inline int g_introFollowupLaunchFrames = 0;
    constexpr int kGiveMeLibertyRetryDelayFrames = 1;
    inline int g_giveMeLibertyRetryFrames = 0;
    inline char g_giveMeLibertyRetrySyntheticKey[9] = {};
    inline bool g_giveMeLibertyLm1RestartPassthroughLogged = false;
    inline bool SafeReadPlayerAlive();
    inline bool HasReachedEightballHideout();
    inline bool HasLuigisGirlsStarted();
    inline bool LaunchApplyPlayerSkinStub(const char* skinName,
                                          bool clearWanted,
                                          const char* reason,
                                          bool clearWeapons = false);
    inline void LaunchGiveMeLibertyFailFastRestoreStub();

    inline void ResetIntroSequenceValidationState() {
        g_introEightballStarted = false;
        g_introGmlSplitGuardFrames = 0;
        g_introFollowupLaunchFrames = 0;
        g_giveMeLibertyRetryFrames = 0;
        g_gmlToLm1CleanupApplied = false;
        g_giveMeLibertyLm1RestartPassthroughLogged = false;
        std::memset(g_giveMeLibertyRetrySyntheticKey, 0,
                    sizeof(g_giveMeLibertyRetrySyntheticKey));
    }

    inline void MarkIntroEightballStarted() {
        if (g_introBankSceneFramesLeft > 0) {
            g_introBankSceneFramesLeft = 0;
            Logger::Log("INTRO bank scene watch stopped: EIGHT/GML started");
        }
        g_introSkipPumpFrames = 0;
        g_introEightballStarted = true;
    }

    inline void ArmIntroGmlSplitGuard() {
        g_introGmlSplitGuardFrames = kIntroGmlSplitGuardDelayFrames;
        Logger::Log("Armed INTRO->GML/LM1 chain guard for %d frames",
                    kIntroGmlSplitGuardDelayFrames);
    }

    inline void TryApplyIntroGmlSplitGuard() {
        if (g_introGmlSplitGuardFrames <= 0) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        --g_introGmlSplitGuardFrames;
        if (g_introGmlSplitGuardFrames > 0) {
            return;
        }

        __try {
            float& splitMode = *reinterpret_cast<float*>(
                ScriptRuntime::ScriptSpace() + kEightSplitModeGlobalOffset);
            const float previous = splitMode;
            splitMode = 0.0f;
            Logger::Log("INTRO->GML/LM1 chain guard: unused_1 %.1f -> 0.0 so EIGHT continues into LM1",
                        previous);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("INTRO->GML chain guard: unused_1 write failed (0x%08lX)",
                        GetExceptionCode());
        }
    }

    inline void ArmIntroFollowupLaunchWatch() {
        if (g_introEightballStarted) {
            g_introFollowupLaunchFrames = 0;
            return;
        }
        if (g_introFollowupLaunchFrames <= 0) {
            g_introFollowupLaunchFrames = kIntroFollowupLaunchDelayFrames;
            Logger::Log("Armed INTRO follow-up mission fallback for %d frames",
                        kIntroFollowupLaunchDelayFrames);
        }
    }

    inline void TryApplyIntroFollowupLaunchWatch() {
        if (g_introFollowupLaunchFrames <= 0) {
            return;
        }
        if (g_introEightballStarted) {
            g_introFollowupLaunchFrames = 0;
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        if (!ScriptRuntime::IsIntroSequenceSyntheticKey(syntheticKey) ||
            RunState::LastLaunchedMission() != 2) {
            g_introFollowupLaunchFrames = 0;
            return;
        }

        --g_introFollowupLaunchFrames;
        if (g_introFollowupLaunchFrames > 0) {
            return;
        }

        if (MenuPatch::QueueIntroGmlFallbackMissionLaunch()) {
            MarkIntroEightballStarted();
            Logger::Log("INTRO follow-up fallback launched APGMLIB EIGHT chain directly");
        } else {
            Logger::Log("INTRO follow-up fallback launch failed; re-arming");
            g_introFollowupLaunchFrames = kIntroFollowupLaunchDelayFrames;
        }
    }

    inline void ArmGiveMeLibertyRetry(const char* syntheticKey, const char* reason) {
        if (!syntheticKey || syntheticKey[0] == '\0') {
            return;
        }
        if (!ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(syntheticKey)) {
            return;
        }
        std::memset(g_giveMeLibertyRetrySyntheticKey, 0,
                    sizeof(g_giveMeLibertyRetrySyntheticKey));
        std::memcpy(g_giveMeLibertyRetrySyntheticKey,
                    syntheticKey,
                    std::min<std::size_t>(std::strlen(syntheticKey), 8));
        g_giveMeLibertyRetryFrames = kGiveMeLibertyRetryDelayFrames;
        Logger::Log("Give Me Liberty retry armed for synthetic key='%s' in %d frames (%s)",
                    g_giveMeLibertyRetrySyntheticKey,
                    kGiveMeLibertyRetryDelayFrames,
                    reason ? reason : "unspecified");
    }

    inline void TryApplyGiveMeLibertyFailedCriticalMissionSkip() {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        if (!ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(syntheticKey)) {
            return;
        }
        const int lastMission = RunState::LastLaunchedMission();
        if (lastMission != 2 && lastMission != 21) {
            return;
        }
        if (!ScriptRuntime::AlreadyRunningAMissionScript()) {
            return;
        }

        std::uint8_t wbState = GameAddr::kWbStatePlaying;
        if (!GameAddr::TryReadPlayerWbState(&wbState) ||
            wbState != GameAddr::kWbStateFailedCriticalMission) {
            g_giveMeLibertyLm1RestartPassthroughLogged = false;
            return;
        }

        const bool reachedHideout = HasReachedEightballHideout();
        const bool luigisGirlsStarted = HasLuigisGirlsStarted();

        // Once EIGHT has crossed into the LM1 half, failures belong to Luigi's
        // Girls. The SCM owns those critical restarts; resetting WBState here
        // misclassifies Misty deaths as GML failures and relaunches the bridge.
        if (lastMission == 21 && (reachedHideout || luigisGirlsStarted)) {
            if (!g_giveMeLibertyLm1RestartPassthroughLogged) {
                g_giveMeLibertyLm1RestartPassthroughLogged = true;
                Logger::Log("Give Me Liberty fail-fast skipped: LM1 state reached_hideout=%d luigis_started=%d; letting SCM critical restart run (synthetic='%.8s')",
                            reachedHideout ? 1 : 0,
                            luigisGirlsStarted ? 1 : 0,
                            syntheticKey ? syntheticKey : "");
            }
            return;
        }

        char retryKey[9] = {};
        std::memcpy(retryKey,
                    syntheticKey,
                    std::min<std::size_t>(std::strlen(syntheticKey), 8));

        if (!GameAddr::TryWritePlayerWbState(GameAddr::kWbStatePlaying)) {
            Logger::Log("Give Me Liberty fail-fast: failed to reset WBState; retry not armed");
            return;
        }

        LaunchGiveMeLibertyFailFastRestoreStub();
        ArmGiveMeLibertyRetry(retryKey, "critical restart skipped");
        Logger::Log("Give Me Liberty fail-fast: WBState %u -> PLAYING, retry key='%s' reached_hideout=%d luigis_started=%d",
                    static_cast<unsigned>(wbState),
                    retryKey,
                    reachedHideout ? 1 : 0,
                    luigisGirlsStarted ? 1 : 0);
    }

    inline void TryApplyGiveMeLibertyRetry() {
        if (g_giveMeLibertyRetryFrames <= 0) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        if (MenuMgr::CurrentPage() != MenuPage::None) {
            return;
        }
        if (ScriptRuntime::AlreadyRunningAMissionScript()) {
            return;
        }
        if (!PlayerPed() || !SafeReadPlayerAlive()) {
            return;
        }
        if (ScriptRuntime::IsPlayerOnMission()) {
            ScriptRuntime::SetPlayerOnMission(false);
            ScriptRuntime::FailCurrentMission() = 0;
            Logger::Log("Give Me Liberty retry: cleared stale player-on-mission flag before relaunch");
        }

        --g_giveMeLibertyRetryFrames;
        if (g_giveMeLibertyRetryFrames > 0) {
            return;
        }

        char retryKey[9] = {};
        std::memcpy(retryKey, g_giveMeLibertyRetrySyntheticKey, sizeof(retryKey));
        ResetIntroSequenceValidationState();
        const bool launched = MenuPatch::QueueGiveMeLibertyRetryMissionLaunch();
        if (launched) {
            MarkIntroEightballStarted();
        }

        if (launched) {
            Logger::Log("Give Me Liberty retry launched synthetic key='%s'",
                        retryKey);
        } else {
            Logger::Log("Give Me Liberty retry launch failed for synthetic key='%s'; re-arming",
                        retryKey);
            std::memcpy(g_giveMeLibertyRetrySyntheticKey, retryKey,
                        sizeof(g_giveMeLibertyRetrySyntheticKey));
            g_giveMeLibertyRetryFrames = kGiveMeLibertyRetryDelayFrames;
        }
    }

    // Arm the ASI half of the vanilla-style intro boot: the SCM side (the
    // patched ap_boot_intro branch) launches INTRO itself and sets unused_1,
    // so unlike the pause-menu APINTRO path there is no delayed trigger and
    // no ConfigureMissionLaunch — but the watches, the engine-launch
    // exemptions for the INTRO -> EIGHT chain and the pending GML validation
    // must match what the menu launch would have armed.
    // Frames spent holding the fallback safehouse warp while waiting for the
    // engine restart that consumes the boot-intro patch.
    inline int g_bootIntroWaitFrames = 0;

    inline void ArmBootIntroSequence() {
        g_bootIntroWaitFrames = 0;
        ArmIntroBankSceneWatch();
        // NOTE: no ArmIntroTeleportWatch here — the vanilla boot CREATEs the
        // player AT the bridge, so the watch would fire on frame one and kill
        // the bank-scene streaming pump before the cutscene even loads.
        ResetIntroSequenceValidationState();
        Hooks::g_allowNextEngineIntroLaunch = true;   // consumed by INTRO, arms EIGHT
        Hooks::g_allowNextEngineEightballLaunch = false;
        RunState::SetLastLaunchedMission(2);
        RunState::SetPendingValidationMission(-1, "APINTRO");
        Logger::Log("Boot-intro sequence armed: INTRO boots vanilla-style, GML validation pending");
    }

    // Tear the boot-intro state back down. Fired when the run start turned out
    // NOT to restart the engine (the safehouse warp completed in the live
    // world) or when the SCM selector pattern could not be patched: without
    // this, g_allowNextEngineIntroLaunch and the pending "APINTRO" validation
    // would block the mission selector forever ("FINISH CURRENT MISSION").
    inline void DisarmBootIntroSequence(const char* reason) {
        Hooks::g_patchApBootIntroLaunch = false;
        Hooks::g_allowNextEngineIntroLaunch = false;
        Hooks::g_allowNextEngineEightballLaunch = false;
        g_introSkipPumpFrames = 0;
        g_introBankSceneFramesLeft = 0;
        g_introTeleportArmed = false;
        ResetIntroSequenceValidationState();
        if (ScriptRuntime::IsIntroSequenceSyntheticKey(
                RunState::PendingValidationSyntheticMission())) {
            RunState::ClearPendingValidationMission();
            RunState::SetLastLaunchedMission(-1);
        }
        Logger::Log("Boot-intro sequence disarmed (%s)", reason);
    }

    // An interrupted intro/GML run leaves the synthetic "APINTRO" validation
    // pending for the rest of the session (the validation watch re-arms it for
    // a retry that, with the eightball loop dead, never comes). That pending
    // state blocks the mission selector ("FINISH CURRENT MISSION") and holds
    // the persistence mirrors shut while the player free-roams. Clear it once
    // the world has been pure freeroam for ~10 s: no mission script, no armed
    // intro watch, nothing in flight.
    inline int g_introPendingIdleFrames = 0;
    inline void TryClearStaleIntroPending() {
        if (!ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(
                RunState::PendingValidationSyntheticMission())) {
            g_introPendingIdleFrames = 0;
            return;
        }
        if (ScriptRuntime::AlreadyRunningAMissionScript() ||
            g_manualIntroTriggerFrames > 0 ||
            g_introBankSceneFramesLeft > 0 ||
            g_introTeleportArmed ||
            g_introGmlSplitGuardFrames > 0 ||
            g_introFollowupLaunchFrames > 0 ||
            g_giveMeLibertyRetryFrames > 0 ||
            Hooks::g_allowNextEngineIntroLaunch ||
            Hooks::g_allowNextEngineEightballLaunch ||
            Hooks::g_patchApBootIntroLaunch) {
            g_introPendingIdleFrames = 0;
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
            return;
        }
        if (++g_introPendingIdleFrames < 600) {
            return;
        }
        g_introPendingIdleFrames = 0;
        RunState::ClearPendingValidationMission();
        RunState::SetLastLaunchedMission(-1);
        ResetIntroSequenceValidationState();
        QueuePlayerOutfitRestore("stale intro/GML cleared");
        Logger::Log("Stale intro/GML pending validation cleared after ~10 s of freeroam; mission selector unblocked");
    }

}
