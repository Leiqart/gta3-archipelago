#pragma once

#include <cstddef>
#include <cstdint>

#include "GameAddresses.h"

// Minimal struct accessors for the bits of CMenuManager we touch.
// Sourced from DK22Pac/plugin-sdk plugin_III/game_III/CMenuManager.h.

enum class MenuPage : std::int32_t {
    None               = 0,
    Stats              = 1,
    NewGame            = 2,
    Briefs             = 3,
    ControllerSettings = 4,
    SoundSettings      = 5,
    DisplaySettings    = 6,
    LanguageSettings   = 7,
    ChooseLoadSlot     = 8,
    ChooseDeleteSlot   = 9,
    NewGameReload      = 10,
    LoadSlotConfirm    = 11,
    DeleteSlotConfirm  = 12,
    DebugMenu          = 18,
    StartMenu          = 51,
    PauseMenu          = 52,
    ArchipelagoMissionList = 56,
    Archipelago        = 57,
    ArchipelagoMissions = 58,
};

namespace MenuMgr {
    // FrontEndMenuManager singleton — we observed self=0x008F59D8 at runtime,
    // matching the v1.1 plugin-sdk symbol.
    constexpr std::uintptr_t kSingletonCompile = 0x008F59D8;

    constexpr std::ptrdiff_t kOff_CurrentPage  = 0x548;
    constexpr std::ptrdiff_t kOff_CurrentEntry = 0x54C;

    inline std::uint8_t* Base() {
        return static_cast<std::uint8_t*>(GameAddr::Translate(kSingletonCompile));
    }

    inline MenuPage CurrentPage() {
        return *reinterpret_cast<MenuPage*>(Base() + kOff_CurrentPage);
    }

    inline std::int32_t CurrentEntry() {
        return *reinterpret_cast<std::int32_t*>(Base() + kOff_CurrentEntry);
    }

    inline void SetCurrentPage(MenuPage page) {
        *reinterpret_cast<MenuPage*>(Base() + kOff_CurrentPage) = page;
    }

    inline void SetCurrentEntry(std::int32_t entry) {
        *reinterpret_cast<std::int32_t*>(Base() + kOff_CurrentEntry) = entry;
    }
}

#pragma pack(push, 1)
struct CMenuEntry {                // 0x14 bytes
    std::int32_t m_nAction;        // MENUACTION_* (0=NOTHING, 2=CHANGEMENU, 25=NEWGAME, ...)
    char         m_EntryName[8];   // GXT key for the displayed label
    std::int32_t m_nSaveSlot;
    std::int32_t m_nTargetMenu;    // when action == CHANGEMENU, the page to jump to
};
static_assert(sizeof(CMenuEntry) == 0x14, "CMenuEntry size mismatch");

struct CMenuScreen {               // 0x184 bytes
    char         m_ScreenName[8];  // GXT key for the page title
    std::int32_t unk;
    std::int32_t m_nPreviousPage;
    std::int32_t m_nPreviousGamePage;
    std::int32_t m_nParentEntry;
    std::int32_t m_nParentGameEntry;
    CMenuEntry   m_aEntries[18];   // NUM_ENTRIES = 18
};
static_assert(sizeof(CMenuScreen) == 0x184, "CMenuScreen size mismatch");
#pragma pack(pop)

namespace Screens {
    constexpr int kCount = 59;

    inline CMenuScreen* All() {
        return static_cast<CMenuScreen*>(GameAddr::Translate(GameAddr::aScreens));
    }

    inline CMenuScreen* Page(MenuPage p) {
        return &All()[static_cast<int>(p)];
    }
}

// Subset of eMenuAction we care about — extracted from plugin-sdk header.
enum MenuAction : std::int32_t {
    MENUACTION_NOTHING    = 0,
    MENUACTION_CHANGEMENU = 2,
    MENUACTION_NEWGAME    = 25,
    // Requests frontend shutdown immediately, regardless of the current page.
    // Verified from re3's Frontend.cpp enum/order: value 49.
    MENUACTION_RESUME_FROM_SAVEZONE = 49,
    // Verified at runtime via the vanilla PauseMenu dump: slot 0 FEM_RES uses
    // action 92, target 0 — the engine's canonical "close menu, resume game".
    MENUACTION_RESUME     = 92,
};
