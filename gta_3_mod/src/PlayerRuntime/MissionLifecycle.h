#pragma once
// PlayerRuntime: mission enter/exit hooks for mission-specific runtime cases.

namespace PlayerRuntime {
    enum class MissionExitReason {
        Passed,
        Failed,
        FailedDeath,
        Quit,
        Aborted,
    };

    inline const char* MissionExitReasonName(MissionExitReason reason) {
        switch (reason) {
            case MissionExitReason::Passed:      return "passed";
            case MissionExitReason::Failed:      return "failed";
            case MissionExitReason::FailedDeath: return "failed-death";
            case MissionExitReason::Quit:        return "quit";
            case MissionExitReason::Aborted:     return "aborted";
            default:                             return "unknown";
        }
    }

    inline bool OnMissionEnter(int actualIndex,
                               ScriptRuntime::MissionLaunchVariant variant,
                               const char* syntheticKey,
                               const char* displayKey,
                               std::uint32_t scriptIp,
                               const char* reason) {
        (void)scriptIp;
        float splitMode = 0.0f;
        switch (variant) {
            case ScriptRuntime::MissionLaunchVariant::IntroToLuigisGirls:
                splitMode = 1.0f;
                break;
            case ScriptRuntime::MissionLaunchVariant::GiveMeLibertySplit:
                // APGMLIB/APINTRO are one AP check for the whole vanilla
                // EIGHT chain: Give Me Liberty, then Luigi's Girls / LM1.
                // Start at 1.0 so EIGHT's mission_start prison-redress block
                // (8ball.sc "IF unused_1 > 0.5 AND unused_1 < 1.5", l.162) runs
                // at GML start — Claude wears PLAYERP (prison) like the mod
                // intends, instead of the PLAYERX skin the intro left on. The
                // split guard (ArmIntroGmlSplitGuard, fires ~30 frames later)
                // resets unused_1 to 0.0 BEFORE the LM1 branch at l.1939, so the
                // chain still continues into Luigi's Girls instead of exiting
                // after GML. OnMissionEnter runs in the StartNewScript hook
                // before EIGHT executes opcode 0, so l.162 sees this 1.0.
                splitMode = 1.0f;
                break;
            case ScriptRuntime::MissionLaunchVariant::LuigisGirlsDirect:
                splitMode = 2.0f;
                break;
            default:
                break;
        }

        __try {
            *reinterpret_cast<float*>(
                ScriptRuntime::ScriptSpace() + kEightSplitModeGlobalOffset) = splitMode;
            if (actualIndex == 21 ||
                variant == ScriptRuntime::MissionLaunchVariant::IntroToLuigisGirls) {
                const std::int32_t lm1Phase =
                    variant == ScriptRuntime::MissionLaunchVariant::LuigisGirlsDirect
                        ? 1 : 0;
                *reinterpret_cast<std::int32_t*>(
                    ScriptRuntime::ScriptSpace() + kEightReachedHideoutGlobalOffset) = lm1Phase;
                *reinterpret_cast<std::int32_t*>(
                    ScriptRuntime::ScriptSpace() + kLuigisGirlsStartedGlobalOffset) = lm1Phase;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("OnMissionEnter raised exception 0x%08lX for key='%.8s' actual=%d",
                        GetExceptionCode(),
                        displayKey ? displayKey : "",
                        actualIndex);
            return false;
        }

        Logger::Log("OnMissionEnter: key='%.8s' actual=%d variant=%d synthetic='%.8s' reason=%s",
                    displayKey ? displayKey : "",
                    actualIndex,
                    static_cast<int>(variant),
                    syntheticKey ? syntheticKey : "",
                    reason ? reason : "unspecified");

        if (actualIndex == 21 &&
            variant == ScriptRuntime::MissionLaunchVariant::GiveMeLibertySplit) {
            MarkIntroEightballStarted();
            ArmIntroGmlSplitGuard();
        }
        return true;
    }

    inline bool OnMissionEnter(const ScriptRuntime::VisibleMissionEntry& entry,
                               std::uint32_t scriptIp,
                               const char* reason) {
        return OnMissionEnter(entry.actualIndex,
                              entry.launchVariant,
                              entry.syntheticValidationKey,
                              entry.displayKey,
                              scriptIp,
                              reason);
    }

    inline void OnMissionExit(int actualIndex,
                              const char* syntheticKey,
                              MissionExitReason exitReason,
                              const char* reason) {
        Logger::Log("OnMissionExit: actual=%d synthetic='%.8s' exit=%s reason=%s",
                    actualIndex,
                    syntheticKey ? syntheticKey : "",
                    MissionExitReasonName(exitReason),
                    reason ? reason : "unspecified");

        if (!ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(syntheticKey)) {
            return;
        }

        const bool lm1RestartOwnedByScm =
            actualIndex == 21 &&
            (HasReachedEightballHideout() || HasLuigisGirlsStarted());
        if ((exitReason == MissionExitReason::Failed ||
             exitReason == MissionExitReason::FailedDeath) &&
            lm1RestartOwnedByScm) {
            Logger::Log("Intro/GML retry skipped: LM1 SCM restart owns this failure reached_hideout=%d luigis_started=%d",
                        HasReachedEightballHideout() ? 1 : 0,
                        HasLuigisGirlsStarted() ? 1 : 0);
            return;
        }

        switch (exitReason) {
            case MissionExitReason::Passed:
                QueuePlayerOutfitRestore(reason ? reason : "intro/GML validated");
                ResetIntroSequenceValidationState();
                break;

            case MissionExitReason::FailedDeath:
                if (ScriptRuntime::IsIntroSequenceSyntheticKey(syntheticKey)) {
                    QueuePlayerOutfitRestore(reason ? reason : "intro/GML player dead");
                }
                ArmGiveMeLibertyRetry(syntheticKey,
                                      reason ? reason : "player dead");
                break;

            case MissionExitReason::Failed:
                ArmGiveMeLibertyRetry(syntheticKey,
                                      reason ? reason : "mission failed");
                break;

            case MissionExitReason::Quit:
            case MissionExitReason::Aborted:
                ResetIntroSequenceValidationState();
                QueuePlayerOutfitRestore(reason ? reason : "intro/GML stopped");
                break;

            default:
                break;
        }
    }
}
