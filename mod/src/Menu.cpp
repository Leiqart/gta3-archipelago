#include <cstring>
#include "MinHook.h"
#include "CMenuManager.h"
#include "CText.h"
#include "ClientLink.h"
#include "Log.h"
#include "Menu.h"

namespace {
    // Shape of CText::Get; the original stays callable through this pointer.
    using TextGet = const wchar_t*(__thiscall*)(void* self, const char* key);
    TextGet g_originalTextGet = nullptr;

    // Runs instead of CText::Get: the Start Game button shows the connection state, every other text is unchanged.
    const wchar_t* __fastcall OurTextGet(void* self, void*, const char* key) {
        if (key && std::strcmp(key, "FEN_STA") == 0) return ClientLink::IsConnected() ? L"Run Archipelago" : L"Not connected";
        return g_originalTextGet(self, key);
    }

    // The Start Game button in plugin-sdk's aScreens, the table of every menu page: row 0 of the main menu, row 1 of the pause menu.
    CMenuScreen::CMenuEntry& MainMenuButton() { return aScreens[MENUPAGE_START_MENU].m_aEntries[0]; }
    CMenuScreen::CMenuEntry& PauseMenuButton() { return aScreens[MENUPAGE_PAUSE_MENU].m_aEntries[1]; }

    // In game we are already in the Archipelago run: the pause menu loses its Start Game row, and the rows below it move up one.
    void RemovePauseStartRow() {
        CMenuScreen& pause = aScreens[MENUPAGE_PAUSE_MENU];
        for (int row = 2; row < NUM_ENTRIES; ++row) {
            CMenuScreen::CMenuEntry& entry = pause.m_aEntries[row];
            // Each sub-page remembers which pause row to highlight when the player comes back: follow the move.
            if (entry.m_nAction == MENUACTION_CHANGEMENU && entry.m_nTargetMenu >= 0 && entry.m_nTargetMenu < NUM_MENU_PAGES) {
                CMenuScreen& subPage = aScreens[entry.m_nTargetMenu];
                if (subPage.m_nParentGameEntry == row) subPage.m_nParentGameEntry = row - 1;
            }
            pause.m_aEntries[row - 1] = entry;
        }
        pause.m_aEntries[NUM_ENTRIES - 1] = {};
    }

    // Makes a button open a page; pointing it to its own page makes a click do nothing.
    void OpenPage(CMenuScreen::CMenuEntry& button, int page) {
        button.m_nAction = MENUACTION_CHANGEMENU;
        button.m_nTargetMenu = page;
    }

    // Makes a button start a new game, like the YES of the vanilla "start a new game?" page (MENUPAGE_NEW_GAME_RELOAD).
    void StartNewGame(CMenuScreen::CMenuEntry& button) {
        button.m_nAction = MENUACTION_NEWGAME;
        button.m_nTargetMenu = MENUPAGE_NEW_GAME_RELOAD;
    }

    // plugin-sdk only knows aScreens in 1.0, so check both buttons before we ever write into the game's memory.
    bool StartButtonsFound() {
        return std::strcmp(MainMenuButton().m_EntryName, "FEN_STA") == 0 && std::strcmp(PauseMenuButton().m_EntryName, "FEN_STA") == 0;
    }
}

bool Menu::Install() {
    if (!StartButtonsFound()) {
        Log::Write("Start Game buttons not found: is this gta3.exe 1.0?");
        return false;
    }
    // gaddrof gives CText::Get's address in the running gta3.exe: plugin-sdk picks it by game version.
    void* const textGet = reinterpret_cast<void*>(gaddrof(CText::Get));
    if (MH_CreateHook(textGet, reinterpret_cast<void*>(&OurTextGet), reinterpret_cast<void**>(&g_originalTextGet)) != MH_OK ||
        MH_EnableHook(textGet) != MH_OK) {
        Log::Write("Menu hook failed");
        return false;
    }
    RemovePauseStartRow();
    Log::Write("Menu installed");
    return true;
}

// Connected: the button starts a new game, which plays the intro then Give Me Liberty. Not connected: a click does nothing.
void Menu::Update() {
    if (ClientLink::IsConnected()) StartNewGame(MainMenuButton());
    else OpenPage(MainMenuButton(), MENUPAGE_START_MENU);
}
