#pragma once

namespace Hooks {
    bool Init();
    void Shutdown();
    // Persistent: when true, every engine-initiated mission launch (any
    // call to CTheScripts::StartNewScript with ip == SIZE_MAIN_SCRIPT that
    // isn't authorized by our own user-driven launcher) has its bytecode
    // rewritten in-place to TERMINATE_THIS_SCRIPT. Set when the user
    // clicks "Run Archipelago" so the boot script can't auto-spawn INTRO
    // or LM1 / RC challenges / etc.; Claude lands in pure freeroam.
    extern bool g_blockEngineMissionLaunches;
    // One-shot escape hatch for the Intro sequence: when true, the next
    // engine-initiated mission launch is treated as the queued INTRO launch
    // even if the bytecode scan cannot resolve a SCRIPT_NAME yet.
    extern bool g_allowNextEngineIntroLaunch;
    // One-shot escape hatch for the Intro sequence: when true, the next
    // engine-initiated EIGHT launch is allowed through even if the general
    // auto-launch blocker is armed.
    extern bool g_allowNextEngineEightballLaunch;
    // One-shot arm: when set, the next time the engine creates the main
    // script (StartNewScript(0)) the hook scans ScriptSpace[0..131072) for
    // the LOAD_AND_LAUNCH_MISSION_INTERNAL 2 opcode (17 04 04 02) and
    // overwrites it with NOPs so the main thread skips INTRO. Cleared on
    // first hit. Must be armed AFTER main.scm has been loaded but BEFORE
    // the main thread runs its first opcode — the StartNewScript(0) call
    // sits in exactly that window.
    extern bool g_patchMainThreadIntroSkip;
    // One-shot arm: when set, the next StartNewScript(0) scans the AP boot
    // selector in main_ap.scm — the IS_INT_VAR_EQUAL_TO_NUMBER compare of
    // unused_2 (&144) against the 0xDEADBEEF sentinel followed by
    // GOTO_IF_FALSE — and flips the branch to GOTO_IF_TRUE, routing the boot
    // through the vanilla-style intro launch (ap_boot_intro) instead of the
    // freeroam setup. Armed at run start while Give Me Liberty is not yet
    // validated, so fresh runs open exactly like a vanilla new game.
    extern bool g_patchApBootIntroLaunch;
}
