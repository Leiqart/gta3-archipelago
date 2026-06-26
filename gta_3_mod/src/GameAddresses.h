#pragma once

#include <windows.h>
#include <cstdint>

namespace GameAddr {
    // GTA III v1.1 (US/Steam, gta3.exe md5 6f2fdac3660a130a2ca042e2243327f6)
    // Addresses sourced from DK22Pac/plugin-sdk and re3 script references.
    constexpr std::uintptr_t kCompileBase = 0x00400000;

    constexpr std::uintptr_t CMenuManager_Process              = 0x00485100;
    constexpr std::uintptr_t CMenuManager_DrawFrontEnd         = 0x0047A540;
    constexpr std::uintptr_t CMenuManager_ProcessButtonPresses = 0x004856F0;
    constexpr std::uintptr_t CPed_Teleport                     = 0x004D3E70;
    constexpr std::uintptr_t CTheScripts_StartNewScript        = 0x00439000;
    constexpr std::uintptr_t CRunningScript_RemoveFromList     = 0x00438FB0;
    constexpr std::uintptr_t CRunningScript_AddToList          = 0x00438FE0;

    // Static array of CMenuScreen, 59 pages, each 0x184 bytes.
    constexpr std::uintptr_t aScreens                           = 0x00611930;
    constexpr std::uintptr_t FindPlayerPed_Pointer              = 0x006FB1C8;
    constexpr std::uintptr_t CTheScripts_MultiScriptArray       = 0x006F0558;
    constexpr std::uintptr_t CTheScripts_ScriptSpace            = 0x0074B248;
    constexpr std::uintptr_t CTheScripts_OnAMissionFlag         = 0x008F1B64;
    constexpr std::uintptr_t CTheScripts_NumberOfMissionScripts = 0x0095CC9A;
    constexpr std::uintptr_t CTheScripts_pActiveScripts         = 0x008E2BF4;
    constexpr std::uintptr_t CTheScripts_pIdleScripts           = 0x009430D4;

    // GXT key lookup: wchar_t* CText::Get(this, char const* key) — thiscall.
    // Called ~767 times per frame in the menu, one per displayed string.
    // Verified against gta3.exe md5 6f2fdac3: the dominant call pattern is
    // `mov ecx, 0x00941520; call 0x0052C5A0` (295 sites = TheText.Get(key)).
    // The prologue at 0x0052C5A0 is `mov eax,[esp+4]; push eax; call ...; ret 4`,
    // a clean thiscall wrapper whose first 5 bytes relocate safely for MinHook.
    // (The previous 0x0052C7E0 was mid-instruction inside an unrelated flag
    //  routine — splicing a JMP there corrupted it and crashed at mission load.)
    constexpr std::uintptr_t CText_Get = 0x0052C5A0;
    constexpr std::uintptr_t TheText   = 0x00941520;

    // CStreaming::LoadScene(CVector const&) — cdecl, forces synchronous load
    // of every model around the given position. Used after warps/respawns to
    // avoid the player ending up over an empty map. Addresses sourced from
    // DK22Pac/plugin-sdk plugin_III meta.
    constexpr std::uintptr_t CStreaming_LoadScene             = 0x0040A6D0;
    constexpr std::uintptr_t CStreaming_FlushRequestList      = 0x0040A680;
    constexpr std::uintptr_t CStreaming_LoadAllRequestedModels = 0x0040A440;

    // Mission abort plumbing — mirror re3's CTheScripts::SwitchToMission:
    //   for each (m_bIsMissionScript && m_bDeatharrestEnabled) script:
    //     unwind stack, IP = stack[0], OnAMissionFlag = 0, wake=0,
    //     m_bDeatharrestExecuted = true,
    //     while(!ProcessOneCommand()) ;   // runs deatharrest cleanup
    //     CMessages::ClearMessages();
    // ProcessOneCommand is the thiscall opcode dispatcher; it returns true
    // when the script has terminated. Addresses from DK22Pac plugin_III.
    constexpr std::uintptr_t CRunningScript_ProcessOneCommand = 0x00439500;
    constexpr std::uintptr_t CMessages_ClearMessages          = 0x00529CE0;

    // CCamera v1.1 singleton + fade method. CCamera::Fade(this, float
    // duration_seconds, short direction) — direction=1 fades IN (from
    // black to scene), direction=0 fades OUT. main_story.scm's setup
    // branch normally calls DO_FADE 1000 1 to fade in after placing
    // Claude; if the setup branch is skipped we end up on a black
    // screen forever, so we call this ourselves once the safehouse
    // warp lands Claude.
    constexpr std::uintptr_t TheCamera   = 0x006FACF8;
    constexpr std::uintptr_t CCamera_Fade = 0x0046B370;

    // CGame::playingIntro v1.1: bool that gates the "loading screen" /
    // intro-mode rendering. SET_INTRO_IS_PLAYING 0 (called by
    // main_story.scm's setup branch) flips it false; if that branch is
    // skipped, the engine never leaves the loading screen.
    constexpr std::uintptr_t CGame_playingIntro = 0x0095CF7A;
    // Static bool — true while a mission is being played. Set by
    // LOAD_AND_LAUNCH_MISSION_INTERNAL, cleared by TERMINATE_THIS_SCRIPT
    // when m_bMissionFlag is true. We must re-assert it after our manual
    // launch path, otherwise the engine thinks no mission is active while
    // our new script has m_bMissionFlag=true.
    constexpr std::uintptr_t CTheScripts_bAlreadyRunningAMissionScript = 0x0095CDB3;
    constexpr std::uintptr_t CTheScripts_FailCurrentMission            = 0x0095CD41;

    // CStats::CommercialPassed (int) — RE'd from CBridge::Update at 0x00413AE0
    // (`cmp dword ptr [0x008F4334], 0`), matching re3 Bridge.cpp: when this stat
    // is 0 the Staunton<->Shoreside lift bridge stays STATE_BRIDGE_LOCKED (raised,
    // impassable); when non-zero it becomes operational and cycles. We drive it
    // from the effective map_state so the lift bridge is frozen up until Shoreside
    // is unlocked. (CBridge::State lives at 0x008F2A1C, kept for reference.)
    constexpr std::uintptr_t CStats_CommercialPassed = 0x008F4334;

    // CStats::MissionsPassed (int) — RE'd from the REGISTER_MISSION_PASSED handler
    // (SCM opcode 0x318, jump-table case 0x00447F95): after copying the 8-char
    // mission name it does `inc dword ptr [0x940768]`. This is the only reliable
    // pass-vs-fail signal: a mission that is failed (soft fail, time-out, target
    // lost) or aborted by death NEVER calls REGISTER_MISSION_PASSED, so snapshot
    // this counter at mission launch and a +1 by mission end means "passed".
    constexpr std::uintptr_t CStats_MissionsPassed = 0x00940768;

    // CWorld::Players[0].m_nCollectedPackages (int) — RE'd from the package-pickup
    // code at 0x004312C2: `movzx edx,[0x95CD61](PlayerInFocus); imul edx,0x13C;
    // mov ax,[edx+0x9413A4]` reads collected packages, `[edx+0x9413A8]` the total,
    // right before pushing "CO_ONE" (0x5EDEA8) to the pickup message. For player 0
    // (PlayerInFocus==0) the collected count is simply ds:[0x009413A4].
    constexpr std::uintptr_t CWorld_Players0_CollectedPackages = 0x009413A4;
    constexpr std::uintptr_t CWorld_Players0_TotalPackages     = 0x009413A8;
    // Current player cash / score as shown on the HUD. Verified from gta3.exe
    // v1.1 reward paths: ADD_SCORE and package/taxi/stunt rewards all add to
    // CWorld::Players[0] + 0xAC = 0x0094139C. The previous 0x00941554 never
    // changed when mission rewards landed, so money persistence stayed at $0.
    constexpr std::uintptr_t CWorld_PlayerCash                 = 0x0094139C;
    // CPlayerInfo::m_WBState is 0x2C bytes after m_nMoney in the PC player-info
    // layout (re3 CPlayerInfo validates total size 0x13C). WBSTATE_FAILED_
    // CRITICAL_MISSION is what RESTART_CRITICAL_MISSION sets before the engine
    // waits roughly 4 seconds and fades out.
    constexpr std::uintptr_t CWorld_Players0_WBState           = CWorld_PlayerCash + 0x2C;
    constexpr std::uint8_t   kWbStatePlaying                   = 0;
    constexpr std::uint8_t   kWbStateFailedCriticalMission     = 3;

    // CPed layout (verified against gta3.exe md5 6f2fdac3 via the SCM opcode
    // handlers). The SET_CHAR_HEALTH handler (opcode 0x0223) ends with
    // `fild [esp+..]; fstp [ped+0x2C0]`, so m_fHealth lives at +0x2C0. Zeroing
    // the *player* ped's health drives the engine's own wasted -> closest
    // hospital respawn through CGameLogic::Update — i.e. a real engine death.
    constexpr std::ptrdiff_t CPed_m_fHealth = 0x2C0;
    // CPed::m_fArmour is the float immediately after m_fHealth (re3 Ped.h:436-437
    // declares them adjacent; +0x2C0 for health is verified against the
    // SET_CHAR_HEALTH handler, so armour lands at +0x2C4).
    constexpr std::ptrdiff_t CPed_m_fArmour = 0x2C4;
    // CPed::GiveWeapon (opcode 0x01B2 handler) indexes m_weapons as
    // `[ped + type*0x18 + 0x35C]`, so the CWeapon[] array is at +0x35C with a
    // 0x18-byte stride. CWeapon fields: +0x00 type, +0x04 state,
    // +0x08 ammoInClip, +0x0C ammoTotal. m_currentWeapon (byte) is at +0x498
    // and the slot count is (0x498-0x35C)/0x18 = 13.
    constexpr std::ptrdiff_t CPed_m_weapons        = 0x35C;
    constexpr std::ptrdiff_t CPed_m_currentWeapon  = 0x498;
    constexpr int            kWeaponSlotCount      = 13;
    constexpr int            kWeaponEntryStride    = 0x18;
    constexpr std::ptrdiff_t kWeaponType_Off       = 0x00;
    constexpr std::ptrdiff_t kWeaponAmmoTotal_Off  = 0x0C;
    // CPed::GiveWeapon(eWeaponType type, int ammo) — thiscall, returns slot.
    // Loads the weapon model and tops up ammo; the engine-correct way to put a
    // weapon back into a ped's inventory after a respawn cleared it.
    constexpr std::uintptr_t CPed_GiveWeapon = 0x004CF9B0;

    // CPed::m_pMyVehicle (+0x310): the vehicle the ped is currently in (null on
    // foot). Read by STORE_CAR_CHAR_IS_IN / IS_CHAR_IN_CAR (both load
    // [ped+0x310]). CVehicle::m_fHealth (+0x200): float written by
    // SET_CAR_HEALTH (fstp [veh+0x200]); 1000 = pristine. CAutomobile smokes
    // the engine at low health and ignites the car near zero, so the F11/F12
    // car traps just drop this value.
    constexpr std::ptrdiff_t CPed_m_pMyVehicle  = 0x310;
    constexpr std::ptrdiff_t CVehicle_m_fHealth = 0x200;

    // CPools::ms_pPedPool — pointer to CPool<CPed>. RE'd from gta3.exe md5
    // 6f2fdac3: the ped-pool iteration at 0x00425E20 loads ds:[0x008F2C60],
    // reads m_size at [pool+8], m_byteMap at [pool+4], m_pObjects at [pool+0],
    // and indexes objects by `imul i, 0x5F0` (= sizeof CPed). A slot is occupied
    // when (m_byteMap[i] & 0x80) == 0 (bit 7 is the "free" flag). Used to walk
    // every live ped and zero the health of those near the player.
    constexpr std::uintptr_t CPools_ms_pPedPool = 0x008F2C60;
    constexpr std::size_t    kSizeof_CPed       = 0x5F0;
    constexpr std::ptrdiff_t kCPool_m_pObjects  = 0x00;
    constexpr std::ptrdiff_t kCPool_m_byteMap   = 0x04;
    constexpr std::ptrdiff_t kCPool_m_size      = 0x08;
    constexpr std::uint8_t   kCPool_freeFlag    = 0x80;
    // CPlaceable world position (CMatrix at +0x04, its pos column at +0x30).
    constexpr std::ptrdiff_t CPlaceable_position = 0x34;

    // CMenuManager FrontEndMenuManager (static object). RE'd from the two
    // `mov ecx, 0x8F59D8; call CMenuManager::Process` sites (0x48C8A4 /
    // 0x48E721). m_bWantToRestart (+0x114) gates the main loop's restart
    // block at 0x582F46: when set, the engine tears the session down and
    // re-initialises a playing game (gGameState=8 at 0x8F5838), reloading
    // main.scm through the genuine new-game boot — exactly what the menu's
    // own New Game action does. m_bWantToLoad (+0x454) routes that restart
    // into the savegame loader instead, so it must stay 0 for a fresh boot.
    constexpr std::uintptr_t FrontEndMenuManager         = 0x008F59D8;
    constexpr std::ptrdiff_t kMenuManager_bWantToRestart = 0x114;
    constexpr std::ptrdiff_t kMenuManager_bWantToLoad    = 0x454;

    // Translate a compile-time address (assuming default ImageBase 0x400000)
    // to its current runtime address. Handles ASLR if Windows relocates the exe.
    inline void* Translate(std::uintptr_t compiled) {
        static const std::uintptr_t base =
            reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
        return reinterpret_cast<void*>(compiled - kCompileBase + base);
    }

    inline bool TryReadPlayerCash(int* out) {
        if (!out) {
            return false;
        }
        __try {
            *out = *reinterpret_cast<int*>(Translate(CWorld_PlayerCash));
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            *out = 0;
            return false;
        }
    }

    inline int ReadPlayerCashOrZero() {
        int value = 0;
        TryReadPlayerCash(&value);
        return value;
    }

    inline bool TryReadPlayerWbState(std::uint8_t* out) {
        if (!out) {
            return false;
        }
        __try {
            *out = *reinterpret_cast<std::uint8_t*>(Translate(CWorld_Players0_WBState));
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            *out = kWbStatePlaying;
            return false;
        }
    }

    inline bool TryWritePlayerWbState(std::uint8_t value) {
        __try {
            *reinterpret_cast<std::uint8_t*>(Translate(CWorld_Players0_WBState)) = value;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // Overwrite the player's cash/score (HUD value) directly. SEH-guarded: the
    // address is only valid once the world is up, so a write before then is a
    // no-op rather than a fault. Used to restore the persisted money total on
    // resume (see PlayerRuntime money persistence).
    inline bool TryWritePlayerCash(int value) {
        __try {
            *reinterpret_cast<int*>(Translate(CWorld_PlayerCash)) = value;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // Ask the engine's main loop to restart the game session through its own
    // new-game path (the same flag the menu's New Game action sets). Returns
    // false only if the static object is somehow unmapped.
    inline bool TryRequestGameRestart() {
        __try {
            auto* base = static_cast<std::uint8_t*>(Translate(FrontEndMenuManager));
            *(base + kMenuManager_bWantToLoad)    = 0;
            *(base + kMenuManager_bWantToRestart) = 1;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // CStats::MissionsPassed, SEH-guarded (the stat lives in static data, so a
    // read should never fault, but keep parity with the other accessors).
    inline int ReadMissionsPassedOrZero() {
        __try {
            return *reinterpret_cast<int*>(Translate(CStats_MissionsPassed));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }
}
