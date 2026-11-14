#include <windows.h>
#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "plugin.h"
#include "CCutsceneMgr.h"
#include "CPad.h"
#include "CPlayerInfo.h"
#include "CPlayerPed.h"
#include "CWeaponInfo.h"
#include "CWorld.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "ClientLink.h"
#include "Items.h"
#include "Log.h"
#include "Player.h"

using namespace plugin;

namespace {
    // The least time between two reports, in milliseconds: ammo and money can change many times a second (a gunfight, a taxi fare), and the save file need not hear each change.
    constexpr DWORD kReportEvery = 1000;
    // The player's weapon slots: in GTA III each weapon has its own, its number in eWeaponType (0, unarmed, holds nothing to keep).
    constexpr int kWeaponSlots = 13;

    // The player's state as the save file keeps it; a value the file does not have is left as the game has it.
    struct State {
        std::optional<int> money, health, armour, inHand;
        // Each weapon held, by its number in the game (eWeaponType), with its ammo.
        std::vector<std::pair<int, int>> weapons;
        bool operator==(const State&) const = default;
    };

    // The state the game starts with: the save file's, or what this game sent last, for a new game started without closing the game.
    std::optional<State> g_saved;
    // Whether the game being played has taken the saved money, then the rest of the state: until both, its own state is not the slot's, and is not reported.
    bool g_moneyTaken = false;
    bool g_restored = false;
    // What was last sent to the client (items applied, state), and when; none while the room is not connected, so that it is sent again once it is.
    std::optional<std::pair<int, State>> g_reported;
    DWORD g_reportedAt = 0;
    // When the game may report after giving the saved state: the weapon put in hand takes a moment to be there, and a report sooner would save the one before.
    DWORD g_restoredAt = 0;

    CPlayerInfo& Info() { return CWorld::Players[CWorld::PlayerInFocus]; }

    // True when the player is in the game and free: alive, not arrested, no cutscene, not held by a script. The state is only given and read then.
    bool Free() {
        return FindPlayerPed() && Info().m_nPlayerState == PLAYERSTATE_PLAYING && !CCutsceneMgr::ms_running && !CPad::GetPad(0)->DisablePlayerControls;
    }

    std::string Describe(const State& state) {
        std::string text;
        const auto add = [&text](const std::string& part) { text += (text.empty() ? "" : " ") + part; };
        if (state.money) add("money=" + std::to_string(*state.money));
        if (state.health) add("health=" + std::to_string(*state.health));
        if (state.armour) add("armour=" + std::to_string(*state.armour));
        if (state.inHand) add("in_hand=" + std::to_string(*state.inHand));
        for (const auto& [weapon, ammo] : state.weapons) add("weapon=" + std::to_string(weapon) + ":" + std::to_string(ammo));
        return text;
    }

    // Reads "key=value" words, as Describe writes them; false for a word that is none of them.
    bool Read(std::istringstream& words, State& state) {
        for (std::string word; words >> word;) {
            const std::size_t equals = word.find('=');
            if (equals == std::string::npos) return false;
            const std::string key = word.substr(0, equals);
            std::istringstream value(word.substr(equals + 1));
            int number = -1, ammo = -1;
            char colon = 0;
            if (key == "weapon") {
                if (!(value >> number >> colon >> ammo) || colon != ':' || number <= 0 || number >= kWeaponSlots || ammo < 0) return false;
                state.weapons.emplace_back(number, ammo);
                continue;
            }
            if (!(value >> number) || number < 0) return false;
            if (key == "money") state.money = number;
            else if (key == "health") state.health = number;
            else if (key == "armour") state.armour = number;
            else if (key == "in_hand") state.inHand = number;
            else return false;
        }
        return true;
    }

    // The player's state now, as the game has it.
    State Now() {
        const CPlayerPed& ped = *FindPlayerPed();
        State state;
        state.money = Info().m_nMoney;
        state.health = static_cast<int>(ped.m_fHealth);
        state.armour = static_cast<int>(ped.m_fArmour);
        state.inHand = static_cast<int>(ped.m_aWeapons[ped.m_nCurrentWeapon].m_eWeaponType);
        for (int slot = 1; slot < kWeaponSlots; ++slot) {
            const CWeapon& weapon = ped.m_aWeapons[slot];
            if (weapon.m_eWeaponType != WEAPONTYPE_UNARMED) state.weapons.emplace_back(static_cast<int>(weapon.m_eWeaponType), static_cast<int>(weapon.m_nAmmoTotal));
        }
        return state;
    }

    // Gives the saved money, on the game's first frame and before its items, which add to it.
    void TakeMoney() {
        g_moneyTaken = true;
        if (!g_saved || !g_saved->money) {
            Log::Write("Player: nothing saved yet, the game starts with its own money");
            return;
        }
        // The counter on the screen runs towards the money a few dollars a frame: it starts at the amount too.
        Info().m_nMoney = Info().m_nDisplayMoney = *g_saved->money;
        Log::Write("Player: the game starts with the $" + std::to_string(*g_saved->money) + " the save file kept");
    }

    // Gives the rest of the saved state once the player is free, through the game's own script commands: a weapon's model is loaded before the weapon is given, as a mission does.
    void Restore() {
        g_restored = true;
        g_restoredAt = GetTickCount();
        if (!g_saved || *g_saved == State{}) {
            Log::Write("Player: nothing saved to restore, the player keeps the game's own health and weapons");
            return;
        }
        CPlayerPed& ped = *FindPlayerPed();
        if (g_saved->health) ped.m_fHealth = static_cast<float>(std::max(1, *g_saved->health));
        if (g_saved->armour) ped.m_fArmour = static_cast<float>(*g_saved->armour);
        for (const auto& [weapon, ammo] : g_saved->weapons) {
            const int model = CWeaponInfo::GetWeaponInfo(static_cast<eWeaponType>(weapon))->m_nModelId;
            if (model > 0) Command<Commands::REQUEST_MODEL>(model);
        }
        Command<Commands::LOAD_ALL_MODELS_NOW>();
        for (const auto& [weapon, ammo] : g_saved->weapons) Command<Commands::GIVE_WEAPON_TO_PLAYER>(CWorld::PlayerInFocus, weapon, ammo);
        for (const auto& [weapon, ammo] : g_saved->weapons) {
            const int model = CWeaponInfo::GetWeaponInfo(static_cast<eWeaponType>(weapon))->m_nModelId;
            if (model > 0) Command<Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
        }
        if (g_saved->inHand) Command<Commands::SET_CURRENT_PLAYER_WEAPON>(CWorld::PlayerInFocus, *g_saved->inHand);
        Log::Write("Player: restored " + Describe(*g_saved));
    }

    // One "player <key>=<value> ..." line: what the save file kept, "player" alone for a slot that never played. A game that starts takes it; a game already running keeps its own, which is newer.
    void HandlePlayer(const std::string& line) {
        std::istringstream words(line);
        std::string command;
        State state;
        if (!(words >> command) || !Read(words, state)) {
            Log::Write("Unknown message from the client: " + line);
            return;
        }
        if (g_moneyTaken) return;
        g_saved = state;
        TakeMoney();
    }

    // Runs once per frame: gives the saved state, then tells the client the count of items applied and the player's state, read together, when they change and each time the room connects.
    void Process() {
        // A new game started without closing the game hears no "player" line: it takes what this game sent last.
        if (!g_moneyTaken && g_saved && FindPlayerPed()) TakeMoney();
        if (!g_moneyTaken || !Free()) return;
        if (!g_restored) Restore();
        if (GetTickCount() - g_restoredAt < kReportEvery) return;
        if (!ClientLink::IsConnected()) {
            g_reported.reset();
            return;
        }
        const std::pair<int, State> progress{Items::Applied(), Now()};
        if (g_reported == progress || (g_reported && GetTickCount() - g_reportedAt < kReportEvery)) return;
        const std::string text = "given_index=" + std::to_string(progress.first) + " " + Describe(progress.second);
        if (!ClientLink::Send("progress " + text)) return;
        g_reported = progress;
        g_reportedAt = GetTickCount();
        g_saved = progress.second;
        Log::Write("Player: progress sent, " + text);
    }
}

void Player::Install() {
    ClientLink::On("player", HandlePlayer);
    // initScriptsEvent follows CTheScripts::Init, which every game that starts runs: its state is the game's own until the saved one is given.
    plugin::Events::initScriptsEvent += [] { g_moneyTaken = g_restored = false; };
    plugin::Events::gameProcessEvent += Process;
}
