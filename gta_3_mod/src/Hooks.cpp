#include "Hooks.h"

#include <windows.h>
#include <MinHook.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "Config.h"
#include "GameAddresses.h"
#include "GameStructs.h"
#include "Logger.h"
#include "MenuPatch.h"
#include "PlayerRuntime.h"
#include "RunState.h"
#include "ScriptRuntime.h"
#include "TextOverrides.h"
namespace {
    using ProcessFn = void(__fastcall*)(void* self, void* edx);
    using CTextGetFn = const wchar_t* (__fastcall*)(void* self, void* edx, const char* key);
    using CutsceneModelLookupFn = void* (__cdecl*)(const char* name);
    using CutsceneCreateObjectFn = void* (__cdecl*)(int modelId);
    using CutsceneAddHeadFn = void* (__cdecl*)(void* parentObject, int modelId);
    using CutsceneSetHeadAnimFn = void(__cdecl*)(const char* animName, void* headObject);

    ProcessFn                   g_origProcess         = nullptr;
    ScriptRuntime::StartNewScriptFn g_origStartNewScript = nullptr;
    CTextGetFn                  g_origCTextGet        = nullptr;
    CutsceneModelLookupFn       g_origCutsceneModelLookup = nullptr;
    CutsceneCreateObjectFn      g_origCutsceneCreateObject = nullptr;
    CutsceneAddHeadFn           g_origCutsceneAddHead = nullptr;
    CutsceneSetHeadAnimFn       g_origCutsceneSetHeadAnim = nullptr;
    int                         g_processCount        = 0;
    MenuPage                    g_lastPage            = static_cast<MenuPage>(-1);
    bool                        g_minHookInitialized  = false;
    bool                        g_processHookEnabled  = false;
    bool                        g_scriptHookEnabled   = false;
    bool                        g_textHookEnabled     = false;
    bool                        g_cutsceneModelHookEnabled = false;
    bool                        g_cutsceneHeadHooksEnabled = false;
    bool                        g_cutscenePlayerAliasLogged = false;
    void*                       g_cutscenePlayerObject = nullptr;

    // GTA III SCM opcode 0x004E = TERMINATE_THIS_SCRIPT. When a brand-new
    // mission script starts and its very first byte-pair is this opcode,
    // the engine's first ProcessOneCommand cycle dispatches straight to
    // the terminator, which removes the script from the active list and
    // clears bAlreadyRunningAMissionScript (since m_bMissionFlag was set
    // by LOAD_AND_LAUNCH_MISSION_INTERNAL right after StartNewScript).
    constexpr std::uint8_t kTerminateThisScriptLo = 0x4E;
    constexpr std::uint8_t kTerminateThisScriptHi = 0x00;
    // The SCRIPT_NAME opcode bytes (uint16 little-endian: 0x03A4), followed
    // by an 8-byte script name. INTRO does not place this right at the top of
    // the mission bytecode, so we scan a wider prefix before deciding the
    // script name is unavailable.
    constexpr std::uint8_t kScriptNameOpcodeLo = 0xA4;
    constexpr std::uint8_t kScriptNameOpcodeHi = 0x03;
    constexpr std::ptrdiff_t kScriptNameStart  = 2; // skip the 2-byte opcode
    constexpr std::size_t kScriptNameLength    = 8;
    constexpr std::size_t kScriptNameScanBytes = 512;

    bool TryPatchMainThreadIntroBranch() {
        if (!Hooks::g_patchMainThreadIntroSkip) {
            return false;
        }

        std::uint8_t* mainArea = ScriptRuntime::ScriptSpace();
        if (!mainArea) {
            return false;
        }

        // Scan for IS_PLAYER_PLAYING (opcode 0x0256 = bytes "56 02") followed
        // within the next 6 bytes by GOTO_IF_FALSE (opcode 0x004D = "4D 00").
        // The 6-byte gap covers either uint16-global param encoding (3 bytes:
        // type 02 + offset hi/lo) or uint32-global (5 bytes: type 0A +
        // 4-byte offset). We patch only the FIRST 2 occurrences in
        // bytecode order — these will be near the start of main thread's
        // bytecode, which is exactly the boot branch (lines 639/640 +
        // 650/651 of the disassembly). Other occurrences live deep inside
        // mission scripts and we don't want to touch them.
        const std::size_t mainAreaSize = ScriptRuntime::kSizeMainScript;
        constexpr int kMaxPatches  = 2;
        constexpr int kMaxGapBytes = 6;
        int patched = 0;
        for (std::size_t i = 0;
             i + 2 + kMaxGapBytes + 2 <= mainAreaSize && patched < kMaxPatches;
             ++i) {
            if (mainArea[i] != 0x56 || mainArea[i + 1] != 0x02) {
                continue;
            }
            for (int gap = 2; gap <= 2 + kMaxGapBytes; ++gap) {
                const std::size_t gotoIdx = i + gap;
                if (gotoIdx + 1 >= mainAreaSize) {
                    break;
                }
                if (mainArea[gotoIdx] == 0x4D && mainArea[gotoIdx + 1] == 0x00) {
                    mainArea[gotoIdx] = 0x4C;
                    Logger::Log("Patched IS_PLAYER_PLAYING@%zu + GOTO_IF_FALSE@%zu -> GOTO_IF_TRUE (gap=%d)",
                                i, gotoIdx, gap);
                    ++patched;
                    i = gotoIdx + 1; // advance past this match
                    break;
                }
            }
        }

        if (patched <= 0) {
            return false;
        }

        Hooks::g_patchMainThreadIntroSkip = false;
        Logger::Log("Main-thread INTRO skip patch applied (%d/%d branch(es))",
                    patched, kMaxPatches);
        return true;
    }

    // Flip the AP boot selector in main_ap.scm so the boot runs the
    // vanilla-style intro launch (ap_boot_intro) instead of the freeroam
    // setup. The selector compiles to:
    //   38 00              IS_INT_VAR_EQUAL_TO_NUMBER
    //   02 90 00           global var &144 (unused_2, uint16 offset)
    //   01 EF BE AD DE     int32 immediate 0xDEADBEEF (sentinel, unique)
    //   4D 00 ...          GOTO_IF_FALSE
    // The compare is always false at boot, so flipping 4D -> 4C
    // (GOTO_IF_TRUE) makes the boot fall through into GOTO ap_boot_intro.
    bool TryPatchApBootIntroBranch() {
        if (!Hooks::g_patchApBootIntroLaunch) {
            return false;
        }
        std::uint8_t* mainArea = ScriptRuntime::ScriptSpace();
        if (!mainArea) {
            return false;
        }

        constexpr std::uint8_t kSelectorPattern[] = {
            0x38, 0x00, 0x02, 0x90, 0x00, 0x01, 0xEF, 0xBE, 0xAD, 0xDE,
        };
        constexpr std::size_t kPatternSize = sizeof(kSelectorPattern);
        const std::size_t mainAreaSize = ScriptRuntime::kSizeMainScript;
        for (std::size_t i = 0; i + kPatternSize + 2 <= mainAreaSize; ++i) {
            if (std::memcmp(mainArea + i, kSelectorPattern, kPatternSize) != 0) {
                continue;
            }
            const std::size_t branchIdx = i + kPatternSize;
            if (mainArea[branchIdx] != 0x4D || mainArea[branchIdx + 1] != 0x00) {
                Logger::Log("AP boot selector found@%zu but branch opcode is %02X%02X, not GOTO_IF_FALSE",
                            i, mainArea[branchIdx], mainArea[branchIdx + 1]);
                return false;
            }
            mainArea[branchIdx] = 0x4C;
            Hooks::g_patchApBootIntroLaunch = false;
            Logger::Log("AP boot selector patched@%zu: boot routed through ap_boot_intro (vanilla intro launch)",
                        branchIdx);
            return true;
        }
        Logger::Log("AP boot selector pattern NOT FOUND; boot stays freeroam");
        return false;
    }

    int PatchClaudeOutfitTriggers(std::uint8_t* bytecode,
                                  std::size_t size,
                                  const char* areaName) {
        if (!bytecode || size < 16) {
            return 0;
        }

        constexpr std::uint8_t kUndressCharLo = 0x52; // UNDRESS_CHAR opcode 0x0352
        constexpr std::uint8_t kUndressCharHi = 0x03;
        constexpr std::uint8_t kPlayerName[8] = {
            'P', 'L', 'A', 'Y', 'E', 'R', 0, 0
        };
        constexpr std::uint8_t kPlayerxName[8] = {
            'P', 'L', 'A', 'Y', 'E', 'R', 'X', 0
        };
        constexpr std::size_t kMaxParamScan = 24;

        int patched = 0;
        for (std::size_t i = 0; i + 2 + 8 <= size; ++i) {
            if (bytecode[i] != kUndressCharLo || bytecode[i + 1] != kUndressCharHi) {
                continue;
            }
            const std::size_t end =
                std::min<std::size_t>(size, i + kMaxParamScan);
            for (std::size_t j = i + 2; j + sizeof(kPlayerxName) <= end; ++j) {
                // PLAYERP is the prison outfit used by Give Me Liberty. Leave
                // it intact; only normal Claude PLAYER redress becomes PLAYERX.
                if (std::memcmp(bytecode + j, kPlayerName, sizeof(kPlayerName)) != 0) {
                    continue;
                }
                std::memcpy(bytecode + j, kPlayerxName, sizeof(kPlayerxName));
                ++patched;
                break;
            }
        }

        if (patched > 0) {
            Logger::Log("Patched %d Claude outfit trigger(s) to PLAYERX in %s",
                        patched,
                        areaName ? areaName : "script bytecode");
        }
        return patched;
    }

    int PatchGiveMeLibertySafehouseCameras(std::uint8_t* bytecode,
                                           std::size_t size,
                                           const char* areaName) {
        if (!bytecode || size < 32) {
            return 0;
        }

        // GTA III stores SCM floats as Q11.4 immediates:
        // tag 0x06 + int16(value * 16). These are the old "water tower"
        // safehouse shots in 8ball.sc, replaced with the low save-house shot.
        static constexpr std::uint8_t kHighFixedCamera[] = {
            0x5F, 0x01,
            0x06, 0x04, 0x35,
            0x06, 0x8C, 0xED,
            0x06, 0x32, 0x01,
            0x04, 0x00,
            0x04, 0x00,
            0x04, 0x00,
        };
        static constexpr std::uint8_t kLowFixedCamera[] = {
            0x5F, 0x01,
            0x06, 0x4A, 0x36,
            0x06, 0x85, 0xEC,
            0x06, 0x84, 0x00,
            0x04, 0x00,
            0x04, 0x00,
            0x04, 0x00,
        };
        static constexpr std::uint8_t kHighPointCamera[] = {
            0x60, 0x01,
            0x06, 0x11, 0x35,
            0x06, 0x84, 0xED,
            0x06, 0x32, 0x01,
            0x04, 0x02,
        };
        static constexpr std::uint8_t kLowPointCamera[] = {
            0x60, 0x01,
            0x06, 0x59, 0x36,
            0x06, 0x88, 0xEC,
            0x06, 0x88, 0x00,
            0x04, 0x02,
        };

        struct Replacement {
            const std::uint8_t* from;
            const std::uint8_t* to;
            std::size_t length;
        };
        const Replacement replacements[] = {
            {kHighFixedCamera, kLowFixedCamera, sizeof(kHighFixedCamera)},
            {kHighPointCamera, kLowPointCamera, sizeof(kHighPointCamera)},
        };

        int patched = 0;
        for (const Replacement& replacement : replacements) {
            for (std::size_t i = 0; i + replacement.length <= size; ++i) {
                if (std::memcmp(bytecode + i,
                                replacement.from,
                                replacement.length) != 0) {
                    continue;
                }
                std::memcpy(bytecode + i,
                            replacement.to,
                            replacement.length);
                ++patched;
                i += replacement.length - 1;
            }
        }

        if (patched > 0) {
            Logger::Log("Patched %d Give Me Liberty safehouse camera opcode(s) in %s",
                        patched,
                        areaName ? areaName : "script bytecode");
        }
        return patched;
    }

    bool PatchGiveMeLibertyStartOutfit(std::uint8_t* bytecode,
                                       std::size_t size,
                                       const char* areaName) {
        if (!bytecode || size < 16) {
            return false;
        }

        constexpr std::uint8_t kUndressCharLo = 0x52; // UNDRESS_CHAR opcode 0x0352
        constexpr std::uint8_t kUndressCharHi = 0x03;
        constexpr std::uint8_t kPlayerxName[8] = {
            'P', 'L', 'A', 'Y', 'E', 'R', 'X', 0
        };
        constexpr std::uint8_t kPlayerpName[8] = {
            'P', 'L', 'A', 'Y', 'E', 'R', 'P', 0
        };
        constexpr std::size_t kMaxParamScan = 24;

        // Patch only the first EIGHT redress block. That is the start of Give
        // Me Liberty; later hideout/LM1 redresses stay PLAYERX.
        for (std::size_t i = 0; i + 2 + 8 <= size; ++i) {
            if (bytecode[i] != kUndressCharLo || bytecode[i + 1] != kUndressCharHi) {
                continue;
            }
            const std::size_t end =
                std::min<std::size_t>(size, i + kMaxParamScan);
            for (std::size_t j = i + 2; j + sizeof(kPlayerxName) <= end; ++j) {
                if (std::memcmp(bytecode + j, kPlayerxName, sizeof(kPlayerxName)) != 0) {
                    continue;
                }
                std::memcpy(bytecode + j, kPlayerpName, sizeof(kPlayerpName));
                Logger::Log("Patched Give Me Liberty start outfit to PLAYERP in %s (undress@%zu)",
                            areaName ? areaName : "script bytecode", i);
                return true;
            }
        }

        Logger::Log("Give Me Liberty start outfit patch skipped: first PLAYERX undress not found in %s",
                    areaName ? areaName : "script bytecode");
        return false;
    }

}

bool Hooks::g_blockEngineMissionLaunches = false;
bool Hooks::g_allowNextEngineIntroLaunch = false;
bool Hooks::g_allowNextEngineEightballLaunch = false;
bool Hooks::g_patchMainThreadIntroSkip   = false;
bool Hooks::g_patchApBootIntroLaunch     = false;

namespace {

    // Keep the per-frame handlers OUT of this function. Many PlayerRuntime::Try*
    // helpers contain __try/__except; when inlined here they merge their SEH
    // scopes into this function's dynamically-realigned, EBX-anchored frame
    // (note the `and esp,-8` + `mov esp,ebx; pop ebx` epilogue). An exception
    // caught inside one of those inlined __try blocks during the pause menu
    // corrupts the EBX stack anchor on the SEH unwind, crashing at the epilogue
    // (0xC0000005 reading [EBX] with EBX/ESP garbage). Forcing the handlers to
    // stay as real calls keeps each __try in its own clean frame and leaves this
    // hook a simple, SEH-free function.
#pragma inline_depth(0)
    void __fastcall Hooked_CMenuManager_Process(void* self, void* edx) {
        ++g_processCount;

        const MenuPage page = MenuMgr::CurrentPage();
        if (page != g_lastPage) {
            Logger::Log("Menu page changed: %d -> %d (entry=%d, frame=%d)",
                        static_cast<int>(g_lastPage),
                        static_cast<int>(page),
                        MenuMgr::CurrentEntry(),
                        g_processCount);
            g_lastPage = page;
        }

        MenuPatch::BeginFrame();
        g_origProcess(self, edx);
        const MenuPage afterProcessPage = MenuMgr::CurrentPage();
        if (page == MenuPage::None && afterProcessPage != MenuPage::None) {
            PlayerRuntime::FlushMoneyToRunState("frontend opened");
        }
        MenuPatch::EndFrame();
        PlayerRuntime::TryApplyOfflineResumeGuard();
        PlayerRuntime::TryApplyRunStartOutfitPreload();
        PlayerRuntime::TryPumpIntroSkipFlag();
        PlayerRuntime::TryApplyManualIntroLaunchTrigger();
        PlayerRuntime::TryApplyIntroBankSceneWatch();
        PlayerRuntime::TryApplyIntroGmlSplitGuard();
        PlayerRuntime::TryApplyIntroFollowupLaunchWatch();
        PlayerRuntime::TryApplyGiveMeLibertyFailedCriticalMissionSkip();
        PlayerRuntime::TryApplyGiveMeLibertyRetry();
        PlayerRuntime::TryApplyGiveMeLibertyToLuigisGirlsCleanup();
        PlayerRuntime::TryApplyGiveMeLibertyUnarmedFix();
        PlayerRuntime::TryApplyPendingSafehouseWarp();
        PlayerRuntime::TryApplyStartupApStateReset();
        PlayerRuntime::TryApplyMoneyPersistence();
        PlayerRuntime::TryApplyPlayerPersistence();
        PlayerRuntime::TrySyncStoryUnlockState();
        PlayerRuntime::TryApplyPostLaunchStreamFlush();
        PlayerRuntime::TryApplySelectedCutscenePlayerSkin();
        PlayerRuntime::TryApplyIntroTeleportWatch();
        PlayerRuntime::TryApplyMissionCompletionWatch();
        PlayerRuntime::PushMapStateToScript();
        PlayerRuntime::DebugManager::TickEarlyHotkeys();
        PlayerRuntime::TryApplyDeathRespawnWatch();
        PlayerRuntime::TryApplyPendingOutfitRestore();
        PlayerRuntime::TryApplyBootOutfitRestore();
        PlayerRuntime::TryClearStaleIntroPending();
        PlayerRuntime::TryApplyContactBlipSync();
        PlayerRuntime::TryApplyServiceBlipSync();
        PlayerRuntime::TryApplyMarkerBlockedToast();
        PlayerRuntime::TryApplyResumeReveal();
        MenuPatch::TickMarkerLaunch();
        PlayerRuntime::DebugManager::TickMissionHotkeys();
        PlayerRuntime::TryApplyMissionValidationWatch();
        PlayerRuntime::TryApplyContactBlipCleanup();
        PlayerRuntime::TryPollBridge();
        PlayerRuntime::DebugManager::TickWorldHotkeys();
        PlayerRuntime::TryApplyPackageMilestoneWatch();
    }
#pragma inline_depth()

    CRunningScript* __cdecl Hooked_CTheScripts_StartNewScript(std::uint32_t ip) {
        // Main thread launch (ip == 0) happens right after the engine has
        // loaded main.scm into ScriptSpace and before the main thread's
        // first opcode runs — perfect window to patch the bytecode so
        // INTRO never gets launched. We flip every "IS_PLAYER_PLAYING &528
        // / GOTO_IF_FALSE" pair to "IS_PLAYER_PLAYING &528 / GOTO_IF_TRUE",
        // which inverts the branch: at boot IS_PLAYER_PLAYING returns false
        // and the script now FALLS THROUGH into the safehouse-setup branch
        // (place Claude at (884,-308,7.6), fade in, enable controls, jump
        // to MAIN_3) instead of taking the INTRO branch.
        //
        // Bytecode signature (main_story.scm IR2 lines 639-640 / 650-651):
        //   56 02            IS_PLAYER_PLAYING opcode (0x0256)
        //   02 10 02         param: global var ref, offset 528 = 0x0210
        //   4D 00            GOTO_IF_FALSE opcode (0x004D)
        // We patch byte index 5 from 0x4D to 0x4C (GOTO_IF_TRUE = 0x004C).
        if (ip == 0) {
            PatchClaudeOutfitTriggers(ScriptRuntime::ScriptSpace(),
                                      ScriptRuntime::kSizeMainScript,
                                      "main script");
        }
        if (ip == 0 && Hooks::g_patchMainThreadIntroSkip) {
            if (!TryPatchMainThreadIntroBranch()) {
                Logger::Log("Main-thread launch: IS_PLAYER_PLAYING+GOTO_IF_FALSE pattern NOT FOUND");
            }
        }
        if (ip == 0 && Hooks::g_patchApBootIntroLaunch) {
            if (TryPatchApBootIntroBranch()) {
                // The intro boot owns this game start; the safehouse warp the
                // run start queued as a fallback must not fire mid-intro, and
                // the persisted request is consumed (it only re-fires if this
                // boot dies before Give Me Liberty validates).
                PlayerRuntime::CancelPendingSafehouseWarp();
                RunState::ClearBootIntroRequest();
            } else {
                PlayerRuntime::DisarmBootIntroSequence("selector patch failed at main-thread launch");
            }
        }

        // Non-mission script launches (helper threads at smaller IPs in the
        // main-script chunk) pass through unchanged.
        if (ip != ScriptRuntime::kSizeMainScript) {
            return g_origStartNewScript(ip);
        }

        // DIAGNOSTIC MODE: every launch is allowed; we only log details
        // so we can identify each script the engine creates in the mission
        // slot. The bytecode that LOAD_AND_LAUNCH_MISSION_INTERNAL just
        // copied into ScriptSpace[SIZE_MAIN_SCRIPT..] starts with the
        // mission's setup opcodes. SCRIPT_NAME (opcode 0x03A4 + 8 bytes)
        // is usually present but not necessarily at offset 0 — INTRO puts it
        // much deeper than most mission scripts. Scan a wider prefix before
        // treating the name as unknown.
        std::uint8_t* bytecode =
            ScriptRuntime::ScriptSpace() + ScriptRuntime::kSizeMainScript;
        char name[9] = {};
        for (std::size_t i = 0; i + 1 + kScriptNameLength <= kScriptNameScanBytes; ++i) {
            if (bytecode[i] == kScriptNameOpcodeLo && bytecode[i + 1] == kScriptNameOpcodeHi) {
                std::memcpy(name, bytecode + i + kScriptNameStart, kScriptNameLength);
                break;
            }
        }
        // Keep the AP outfit during normal missions. Cutscene animation groups
        // still ask for a literal "player" model; Hooked_Cutscene_GetModelFromName
        // aliases that missing association to PLAYERX without changing Claude's
        // visible skin. INTRO/EIGHT remain exempt because their SCM already owns
        // the prison/base/AP outfit transitions explicitly.
        if (name[0] == 0 ||
            (_strnicmp(name, "INTRO", 5) != 0 &&
             _strnicmp(name, "EIGHT", 5) != 0)) {
            PatchClaudeOutfitTriggers(bytecode,
                                      ScriptRuntime::kSizeMissionScript,
                                      "mission script");
        }

        // INTRO is special: the engine relies on it running for the
        // loading-state transitions. We never block it — we just
        // leave its bytecode vanilla. The SCM source owns the INTRO -> EIGHT
        // flow.
        const bool hasScriptName = name[0] != 0;
        const bool isIntro = hasScriptName && _strnicmp(name, "INTRO", 5) == 0;
        const bool isEight = hasScriptName && _strnicmp(name, "EIGHT", 5) == 0;
        if (isEight) {
            PatchGiveMeLibertySafehouseCameras(bytecode,
                                               ScriptRuntime::kSizeMissionScript,
                                               "EIGHT mission script");
        }
        const bool allowQueuedIntroLaunch = Hooks::g_allowNextEngineIntroLaunch;
        const bool allowIntroFollowupEight = Hooks::g_allowNextEngineEightballLaunch;
        const bool markerMode =
            Config::UseClassicMissionMarkers() && RunState::IsRunLive();
        const ScriptRuntime::VisibleMissionEntry* markerEntry =
            markerMode ? ScriptRuntime::FindVisibleMissionByThreadName(name) : nullptr;
        const int authorizedMission = ScriptRuntime::PeekAuthorizedMissionLaunch();
        const bool userAuthorized = authorizedMission >= 0;
        const bool giveMeLibertyStart =
            isEight && PlayerRuntime::IsGiveMeLibertyStartMode() &&
            (allowIntroFollowupEight || authorizedMission == 21 ||
             PlayerRuntime::IsIntroSequenceMissionActive());

        if (giveMeLibertyStart) {
            PatchGiveMeLibertyStartOutfit(bytecode,
                                          ScriptRuntime::kSizeMissionScript,
                                          "EIGHT mission script");
            PlayerRuntime::ArmGiveMeLibertyUnarmedFix("EIGHT/GML launch");
        }

        // User pause-menu launches pass through unchanged after AP-specific
        // bytecode fixups above.
        if (userAuthorized) {
            Logger::Log("Mission launch (user-authorized): script='%.8s'", name);
            return g_origStartNewScript(ip);
        }

        // Engine auto-launch. Block everything EXCEPT INTRO when the
        // freeroam-at-safehouse mode armed the block at Run Archipelago
        // click — this catches LM1 (EIGHT), RC challenges, ambient
        // missions, etc., right from the start instead of waiting for
        // the safehouse warp to apply.
        if (Hooks::g_blockEngineMissionLaunches &&
            !isIntro &&
            !allowQueuedIntroLaunch &&
            !allowIntroFollowupEight) {
            bool blockLaunch = true;
            if (markerMode && markerEntry && !markerEntry->bugged &&
                ScriptRuntime::IsMissionEntryUnlocked(*markerEntry)) {
                blockLaunch = false;
            }

            if (blockLaunch) {
                bytecode[0] = kTerminateThisScriptLo;
                bytecode[1] = kTerminateThisScriptHi;
                if (markerMode && markerEntry) {
                    // Tell the player WHY nothing happened on the marker.
                    PlayerRuntime::QueueMarkerBlockedToast(markerEntry);
                    Logger::Log("Engine marker launch BLOCKED: script='%.8s' key='%.8s' unlocked=%d bugged=%d",
                                name,
                                markerEntry->displayKey ? markerEntry->displayKey : "",
                                ScriptRuntime::IsMissionEntryUnlocked(*markerEntry) ? 1 : 0,
                                markerEntry->bugged ? 1 : 0);
                } else {
                    Logger::Log("Engine mission auto-launch BLOCKED: script='%.8s' patched to TERMINATE", name);
                }
                return g_origStartNewScript(ip);
            }
        }

        if (allowQueuedIntroLaunch && isIntro) {
            Hooks::g_allowNextEngineIntroLaunch = false;
            Hooks::g_allowNextEngineEightballLaunch = true;
            Logger::Log("Engine mission auto-launch (queued INTRO allowed once): script='%.8s'", name);
        } else if (allowQueuedIntroLaunch) {
            Logger::Log("Engine mission auto-launch (queued INTRO still waiting): script='%.8s'", name);
        }

        if (allowIntroFollowupEight && isEight) {
            Hooks::g_allowNextEngineEightballLaunch = false;
            PlayerRuntime::OnMissionEnter(
                21,
                ScriptRuntime::MissionLaunchVariant::GiveMeLibertySplit,
                "APINTRO",
                "EIGHT",
                ip,
                "APINTRO engine EIGHT");
            Logger::Log("Engine mission auto-launch (queued EIGHT follow-up allowed once): script='%.8s'", name);
        } else if (allowIntroFollowupEight) {
            Logger::Log("Engine mission auto-launch (queued EIGHT still waiting): script='%.8s'", name);
        }

        Logger::Log("Engine mission auto-launch (allowed): script='%.8s'", name);
        CRunningScript* started = g_origStartNewScript(ip);
        if (started && markerMode && markerEntry && !markerEntry->bugged) {
            RunState::SetLastLaunchedMission(markerEntry->actualIndex);
            if (markerEntry->syntheticValidationKey && markerEntry->syntheticValidationKey[0] != 0) {
                RunState::SetPendingValidationMission(-1, markerEntry->syntheticValidationKey);
            } else {
                RunState::SetPendingValidationMission(markerEntry->actualIndex);
            }
            PlayerRuntime::PreloadObjectivePositions(markerEntry->actualIndex);
            Logger::Log("Engine marker launch tracked: script='%.8s' key='%.8s' actual index=%d",
                        name,
                        markerEntry->displayKey ? markerEntry->displayKey : "",
                        markerEntry->actualIndex);
        }

        // If this is INTRO, fire the script's own cutscene-skip flag by
        // writing 2 to the active INTRO skip global. This offset has moved
        // between SCM builds, so keep it centralized in PlayerRuntime::Core.
        // The INTRO loop polls it every
        // frame; once it reads 2 the
        // cutscene loop jumps to MISSION_2_176 = post-skip cleanup,
        // which lands Claude on the Callahan Bridge and lets the
        // mission terminate normally instead of dragging on for minutes.
        if (isIntro && !allowQueuedIntroLaunch) {
            __try {
                auto* skipFlag = reinterpret_cast<std::int32_t*>(
                    ScriptRuntime::ScriptSpace() + PlayerRuntime::kIntroSkipFlagOffset);
                *skipFlag = 2;
                Logger::Log("INTRO auto-skip: wrote ScriptSpace[%d]=2 to trigger cutscene-skip path",
                            static_cast<int>(PlayerRuntime::kIntroSkipFlagOffset));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("INTRO auto-skip: failed to write skip flag");
            }
        }
        return started;
    }

    // CText::Get(this, key) is thiscall-by-this — fastcall hooks see edx as
    // the unused second register and the real argument shifted to the third
    // slot. We intercept the lookup, route any TextOverrides::Lookup hit
    // straight back to the caller, and fall through to the original for
    // every other key. Engines call this 700+ times per frame for the
    // pause menu, so the check is intentionally minimal: a single strncmp
    // against a short list of our overridden keys.
    bool IsPlayerCutsceneModelName(const char* name) {
        if (!name || _strnicmp(name, "player", 6) != 0) {
            return false;
        }
        // The vanilla helper ignores numeric suffixes on animation/model names.
        // Preserve that behaviour for names such as player1, but never alias
        // PLAYERP or another genuinely different model.
        for (const char* tail = name + 6; *tail; ++tail) {
            if (*tail < '0' || *tail > '9') {
                return false;
            }
        }
        return true;
    }

    bool IsNormalApMissionCutscene() {
        return RunState::LastLaunchedMission() > 21 &&
               !PlayerRuntime::IsIntroSequencePresentationActive() &&
               !PlayerRuntime::IsGiveMeLibertyPhaseActive();
    }

    void* __cdecl Hooked_Cutscene_CreateObject(int modelId) {
        void* object = g_origCutsceneCreateObject(modelId);
        if (modelId == 0) { // PED_PLAYER
            g_cutscenePlayerObject = IsNormalApMissionCutscene() ? object : nullptr;
            if (g_cutscenePlayerObject) {
                Logger::Log("Cutscene PED_PLAYER object tracked: %p", object);
            }
        }
        return object;
    }

    void* __cdecl Hooked_Cutscene_AddHead(void* parentObject, int modelId) {
        if (parentObject && parentObject == g_cutscenePlayerObject &&
            IsNormalApMissionCutscene()) {
            // Returning the parent gives the SCM a valid object-pool handle for
            // its following SET_CUTSCENE_HEAD_ANIM command, while deliberately
            // avoiding CCutsceneHead's constructor that hides the skin's head.
            Logger::Log("Claude PLAYERH suppressed: parent=%p model=%d",
                        parentObject,
                        modelId);
            return parentObject;
        }
        return g_origCutsceneAddHead(parentObject, modelId);
    }

    void __cdecl Hooked_Cutscene_SetHeadAnim(const char* animName, void* headObject) {
        if (headObject && headObject == g_cutscenePlayerObject &&
            IsNormalApMissionCutscene()) {
            Logger::Log("Claude head animation skipped: '%s' object=%p",
                        animName ? animName : "",
                        headObject);
            return;
        }
        g_origCutsceneSetHeadAnim(animName, headObject);
    }

    bool InstallCutsceneHeadHooks() {
        void* createAddress = GameAddr::Translate(GameAddr::Cutscene_CreateObject);
        void* addAddress = GameAddr::Translate(GameAddr::Cutscene_AddHead);
        void* animAddress = GameAddr::Translate(GameAddr::Cutscene_SetHeadAnim);
        if (MH_CreateHook(createAddress,
                          &Hooked_Cutscene_CreateObject,
                          reinterpret_cast<void**>(&g_origCutsceneCreateObject)) != MH_OK ||
            MH_CreateHook(addAddress,
                          &Hooked_Cutscene_AddHead,
                          reinterpret_cast<void**>(&g_origCutsceneAddHead)) != MH_OK ||
            MH_CreateHook(animAddress,
                          &Hooked_Cutscene_SetHeadAnim,
                          reinterpret_cast<void**>(&g_origCutsceneSetHeadAnim)) != MH_OK) {
            Logger::Log("Cutscene head hooks creation failed");
            MH_RemoveHook(createAddress);
            MH_RemoveHook(addAddress);
            MH_RemoveHook(animAddress);
            return false;
        }
        if (MH_EnableHook(createAddress) != MH_OK ||
            MH_EnableHook(addAddress) != MH_OK ||
            MH_EnableHook(animAddress) != MH_OK) {
            Logger::Log("Cutscene head hooks enable failed");
            MH_DisableHook(createAddress);
            MH_DisableHook(addAddress);
            MH_DisableHook(animAddress);
            MH_RemoveHook(createAddress);
            MH_RemoveHook(addAddress);
            MH_RemoveHook(animAddress);
            return false;
        }
        g_cutsceneHeadHooksEnabled = true;
        return true;
    }
    void* __cdecl Hooked_Cutscene_GetModelFromName(const char* name) {
        void* model = g_origCutsceneModelLookup(name);
        if (!IsPlayerCutsceneModelName(name)) {
            return model;
        }

        // INTRO/Give Me Liberty own their PLAYER/PLAYERP transitions. Normal AP
        // missions must always animate the live PLAYERX clump, even when a
        // mission (notably LUIGI2) explicitly loaded a second literal PLAYER
        // model. Returning that successfully found model is what displayed the
        // brown/alternate Claude in the cutscene.
        if (RunState::LastLaunchedMission() <= 21 ||
            PlayerRuntime::IsIntroSequencePresentationActive() ||
            PlayerRuntime::IsGiveMeLibertyPhaseActive()) {
            return model;
        }

        PlayerRuntime::QueueCutscenePlayerSkinApply(name);
        if (PlayerRuntime::g_secondaryPlayerSkinActive && model) {
            return model;
        }

        void* playerx = g_origCutsceneModelLookup("playerx");
        if (playerx) {
            if (!g_cutscenePlayerAliasLogged) {
                g_cutscenePlayerAliasLogged = true;
                Logger::Log("Cutscene model forced: '%s' (%p) -> PLAYERX (%p)",
                            name,
                            model,
                            playerx);
            }
            return playerx;
        }
        return model;
    }
    const wchar_t* __fastcall Hooked_CText_Get(void* self, void* edx, const char* key) {
        if (key) {
            const wchar_t* override = TextOverrides::Lookup(key);
            if (override) {
                return override;
            }
            // Marked label "APKnnnn" (installed by MenuPatch for missions,
            // contacts and regions): recover the real GXT key + state, fetch the
            // real name from the original CText and prefix the state marker.
            const char* originalKey = nullptr;
            char markerState = 0;
            if (TextOverrides::ResolveMarked(key, &originalKey, &markerState)) {
                const wchar_t* name = g_origCTextGet(self, edx, originalKey);
                return TextOverrides::ComposeMissionMarker(markerState, name);
            }
            if (TextOverrides::ResolveUnlockToast(key, &originalKey)) {
                const wchar_t* name = g_origCTextGet(self, edx, originalKey);
                return TextOverrides::ComposeUnlockToast(name);
            }
        }
        return g_origCTextGet(self, edx, key);
    }

}

bool Hooks::Init() {
    if (MH_Initialize() != MH_OK) {
        Logger::Log("MH_Initialize failed");
        return false;
    }
    g_minHookInitialized = true;

    if (MH_CreateHook(GameAddr::Translate(GameAddr::CMenuManager_Process),
                      &Hooked_CMenuManager_Process,
                      reinterpret_cast<void**>(&g_origProcess)) != MH_OK) {
        Logger::Log("MH_CreateHook failed for CMenuManager::Process");
        MH_Uninitialize();
        g_minHookInitialized = false;
        return false;
    }

    if (MH_EnableHook(GameAddr::Translate(GameAddr::CMenuManager_Process)) != MH_OK) {
        Logger::Log("MH_EnableHook failed for CMenuManager::Process");
        MH_RemoveHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        MH_Uninitialize();
        g_minHookInitialized = false;
        return false;
    }
    g_processHookEnabled = true;

    if (MH_CreateHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript),
                      &Hooked_CTheScripts_StartNewScript,
                      reinterpret_cast<void**>(&g_origStartNewScript)) != MH_OK) {
        Logger::Log("MH_CreateHook failed for CTheScripts::StartNewScript");
        MH_DisableHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        g_processHookEnabled = false;
        MH_Uninitialize();
        g_minHookInitialized = false;
        return false;
    }

    if (MH_EnableHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript)) != MH_OK) {
        Logger::Log("MH_EnableHook failed for CTheScripts::StartNewScript");
        MH_RemoveHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        MH_DisableHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        g_processHookEnabled = false;
        MH_Uninitialize();
        g_minHookInitialized = false;
        return false;
    }
    g_scriptHookEnabled = true;

    if (MH_CreateHook(GameAddr::Translate(GameAddr::Cutscene_GetModelFromName),
                      &Hooked_Cutscene_GetModelFromName,
                      reinterpret_cast<void**>(&g_origCutsceneModelLookup)) != MH_OK) {
        Logger::Log("MH_CreateHook failed for cutscene model alias");
        MH_DisableHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        g_scriptHookEnabled = false;
        MH_DisableHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        g_processHookEnabled = false;
        MH_Uninitialize();
        g_minHookInitialized = false;
        return false;
    }
    if (MH_EnableHook(GameAddr::Translate(GameAddr::Cutscene_GetModelFromName)) != MH_OK) {
        Logger::Log("MH_EnableHook failed for cutscene model alias");
        MH_RemoveHook(GameAddr::Translate(GameAddr::Cutscene_GetModelFromName));
        MH_DisableHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        g_scriptHookEnabled = false;
        MH_DisableHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        g_processHookEnabled = false;
        MH_Uninitialize();
        g_minHookInitialized = false;
        return false;
    }
    g_cutsceneModelHookEnabled = true;
    InstallCutsceneHeadHooks();

    if (MH_CreateHook(GameAddr::Translate(GameAddr::CText_Get),
                      &Hooked_CText_Get,
                      reinterpret_cast<void**>(&g_origCTextGet)) != MH_OK) {
        Logger::Log("MH_CreateHook failed for CText::Get (overrides disabled)");
    } else if (MH_EnableHook(GameAddr::Translate(GameAddr::CText_Get)) != MH_OK) {
        Logger::Log("MH_EnableHook failed for CText::Get (overrides disabled)");
        MH_RemoveHook(GameAddr::Translate(GameAddr::CText_Get));
    } else {
        g_textHookEnabled = true;
    }

    Logger::Log("Hooks enabled: menu process, StartNewScript, cutscene PLAYERX alias%s%s",
                g_cutsceneHeadHooksEnabled ? ", Claude head passthrough" : "",
                g_textHookEnabled ? ", CText::Get" : "");
    return true;
}

void Hooks::Shutdown() {
    if (!g_minHookInitialized) {
        return;
    }

    if (g_textHookEnabled) {
        MH_DisableHook(GameAddr::Translate(GameAddr::CText_Get));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CText_Get));
        g_textHookEnabled = false;
    }
    if (g_cutsceneHeadHooksEnabled) {
        MH_DisableHook(GameAddr::Translate(GameAddr::Cutscene_CreateObject));
        MH_DisableHook(GameAddr::Translate(GameAddr::Cutscene_AddHead));
        MH_DisableHook(GameAddr::Translate(GameAddr::Cutscene_SetHeadAnim));
        MH_RemoveHook(GameAddr::Translate(GameAddr::Cutscene_CreateObject));
        MH_RemoveHook(GameAddr::Translate(GameAddr::Cutscene_AddHead));
        MH_RemoveHook(GameAddr::Translate(GameAddr::Cutscene_SetHeadAnim));
        g_cutsceneHeadHooksEnabled = false;
        g_cutscenePlayerObject = nullptr;
    }
    if (g_cutsceneModelHookEnabled) {
        MH_DisableHook(GameAddr::Translate(GameAddr::Cutscene_GetModelFromName));
        MH_RemoveHook(GameAddr::Translate(GameAddr::Cutscene_GetModelFromName));
        g_cutsceneModelHookEnabled = false;
    }
    if (g_scriptHookEnabled) {
        MH_DisableHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CTheScripts_StartNewScript));
        g_scriptHookEnabled = false;
    }
    if (g_processHookEnabled) {
        MH_DisableHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        MH_RemoveHook(GameAddr::Translate(GameAddr::CMenuManager_Process));
        g_processHookEnabled = false;
    }
    MH_Uninitialize();
    g_minHookInitialized = false;
    Logger::Log("Hooks shutdown complete");
}
