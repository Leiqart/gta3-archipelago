#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>
#include "plugin.h"
#include "CMessages.h"
#include "CPad.h"
#include "CRadar.h"
#include "CRunningScript.h"
#include "CText.h"
#include "CTheScripts.h"
#include "CTimer.h"
#include "Log.h"
#include "MainScript.h"
#include "Markers.h"

namespace {
    // The script commands a trigger is found by, as main.scm stores them: 2 bytes of opcode (low byte first), then each argument's type byte and value.
    constexpr char kStartNewScript[] = "\x4F\x00\x01";
    constexpr char kPrintBig[] = "\xBA\x00";
    constexpr char kLaunchMission[] = "\x17\x04\x04";

    // A mission's trigger: the script waiting for the player on its marker, from its first byte to the next script's, with where its title's text key and its mission number are.
    struct Trigger {
        std::size_t start = 0;
        std::size_t end = 0;
        std::size_t titleAt = 0;
        std::size_t numberAt = 0;
    };

    // A mission started from a contact's marker or phone: its number in main.scm, the contact's radar sprite (none for Marty's phone, which has no blip), and its state.
    struct Mission {
        unsigned char number;
        unsigned short sprite;
        bool unlocked = false;
        Trigger trigger;
    };

    // Every story mission started from a contact, in story order; Give Me Liberty and Luigi's Girls are left out, as the intro starts them.
    Mission g_missions[] = {
        {20, RADAR_SPRITE_LUIGI},  // Don't Spank Ma Bitch Up
        {21, RADAR_SPRITE_LUIGI},  // Drive Misty For Me
        {24, RADAR_SPRITE_JOEY},   // Mike Lips Last Lunch
        {25, RADAR_SPRITE_JOEY},   // Farewell 'Chunky' Lee Chong
        {26, RADAR_SPRITE_JOEY},   // Van Heist
        {27, RADAR_SPRITE_JOEY},   // Cipriani's Chauffeur
        {28, RADAR_SPRITE_JOEY},   // Dead Skunk in the Trunk
        {29, RADAR_SPRITE_JOEY},   // The Getaway
        {30, RADAR_SPRITE_TONY},   // Taking Out the Laundry
        {31, RADAR_SPRITE_TONY},   // The Pick-Up
        {32, RADAR_SPRITE_TONY},   // Salvatore's Called a Meeting
        {35, RADAR_SPRITE_SAL},    // Chaperone
        {22, RADAR_SPRITE_LUIGI},  // Pump-Action Pimp
        {23, RADAR_SPRITE_LUIGI},  // The Fuzz Ball
        {33, RADAR_SPRITE_TONY},   // Triads and Tribulations
        {36, RADAR_SPRITE_SAL},    // Cutting the Grass
        {34, RADAR_SPRITE_TONY},   // Blow Fish
        {37, RADAR_SPRITE_SAL},    // Bomb Da Base: Act I
        {38, RADAR_SPRITE_EIGHT},  // Bomb Da Base: Act II
        {39, RADAR_SPRITE_SAL},    // Last Requests
        {44, RADAR_SPRITE_ASUKA},  // Sayonara Salvatore
        {45, RADAR_SPRITE_ASUKA},  // Under Surveillance
        {46, RADAR_SPRITE_ASUKA},  // Paparazzi Purge
        {47, RADAR_SPRITE_ASUKA},  // Payday for Ray
        {54, RADAR_SPRITE_RAY},    // Silence the Sneak
        {48, RADAR_SPRITE_ASUKA},  // Two-Faced Tanner
        {49, RADAR_SPRITE_KENJI},  // Kanbu Bust-out
        {50, RADAR_SPRITE_KENJI},  // Grand Theft Auto
        {51, RADAR_SPRITE_KENJI},  // Deal Steal
        {52, RADAR_SPRITE_KENJI},  // Shima
        {53, RADAR_SPRITE_KENJI},  // Smack Down
        {55, RADAR_SPRITE_RAY},    // Arms Shortage
        {56, RADAR_SPRITE_RAY},    // Evidence Dash
        {57, RADAR_SPRITE_RAY},    // Gone Fishing
        {58, RADAR_SPRITE_RAY},    // Plaster Blaster
        {60, RADAR_SPRITE_DON},    // Liberator
        {61, RADAR_SPRITE_DON},    // Waka-Gashira Wipeout!
        {62, RADAR_SPRITE_DON},    // A Drop in the Ocean
        {59, RADAR_SPRITE_RAY},    // Marked Man
        {67, RADAR_SPRITE_DON},    // Grand Theft Aero
        {68, RADAR_SPRITE_DON},    // Escort Service
        {69, RADAR_SPRITE_DON},    // Decoy
        {70, RADAR_SPRITE_DON},    // Love's Disappearance
        {71, RADAR_SPRITE_ASUKA},  // Bait
        {72, RADAR_SPRITE_ASUKA},  // Espresso-2-Go!
        {73, RADAR_SPRITE_ASUKA},  // S.A.M.
        {79, RADAR_SPRITE_CAT},    // The Exchange
        {15, RADAR_SPRITE_NONE},   // The Crook
        {16, RADAR_SPRITE_NONE},   // The Thieves
        {17, RADAR_SPRITE_NONE},   // The Wife
        {18, RADAR_SPRITE_NONE},   // Her Lover
        {40, RADAR_SPRITE_EL},     // Turismo
        {41, RADAR_SPRITE_EL},     // I Scream, You Scream
        {42, RADAR_SPRITE_EL},     // Trial By Fire
        {43, RADAR_SPRITE_EL},     // Big'N'Veiny
        {63, RADAR_SPRITE_LIZ},    // Bling-Bling Scramble
        {64, RADAR_SPRITE_LIZ},    // Uzi Rider
        {65, RADAR_SPRITE_LIZ},    // Gangcar Round-Up
        {66, RADAR_SPRITE_LIZ},    // Kingdom Come
        {74, RADAR_SPRITE_ICE},    // Uzi Money
        {75, RADAR_SPRITE_ICE},    // Toyminator
        {76, RADAR_SPRITE_ICE},    // Rigged To Blow
        {77, RADAR_SPRITE_ICE},    // Bullion Run
        {78, RADAR_SPRITE_ICE},    // Rumble
    };

    // Test: the marker of the first mission launches the second, under its title (20 and 50: Luigi's marker starts Grand Theft Auto); 0 turns it off.
    constexpr unsigned char kTestMarker = 0;
    constexpr unsigned char kTestLaunched = 50;

    // The mission of the table with this number; nullptr if it is not in it.
    Mission* Find(unsigned char number) {
        for (Mission& mission : g_missions) {
            if (mission.number == number) return &mission;
        }
        return nullptr;
    }

    // Every position where a START_NEW_SCRIPT starts a script in the main part (where the triggers live), sorted; most triggers are started from the missions' code.
    std::vector<std::size_t> ScriptStarts(const std::string& scm) {
        std::vector<std::size_t> starts;
        for (std::size_t at = scm.find(kStartNewScript, 0, 3); at != std::string::npos && at + 7 <= scm.size(); at = scm.find(kStartNewScript, at + 1, 3)) {
            const int position = *reinterpret_cast<const int*>(scm.data() + at + 3);
            if (position > 0 && static_cast<std::size_t>(position) < MainScript::MainSize()) starts.push_back(static_cast<std::size_t>(position));
        }
        std::sort(starts.begin(), starts.end());
        return starts;
    }

    // A mission's trigger: the script holding its LOAD_AND_LAUNCH_MISSION_INTERNAL in the main part, between the script starts around it; all zero when not found.
    Trigger FindTrigger(const std::string& scm, const std::vector<std::size_t>& starts, unsigned char number) {
        const char launch[] = {kLaunchMission[0], kLaunchMission[1], kLaunchMission[2], static_cast<char>(number)};
        const std::size_t at = scm.find(launch, 0, 4);
        if (at == std::string::npos || at >= MainScript::MainSize()) return {};
        const auto next = std::upper_bound(starts.begin(), starts.end(), at);
        if (next == starts.begin()) return {};
        Trigger trigger;
        trigger.start = *(next - 1);
        trigger.end = next != starts.end() ? *next : MainScript::MainSize();
        trigger.numberAt = at + 3;
        // Its title is the last PRINT_BIG before the launch, an 8-byte text key the game looks up in the player's language.
        for (std::size_t title = scm.find(kPrintBig, trigger.start, 2); title < at; title = scm.find(kPrintBig, title + 1, 2)) {
            if (MainScript::IsKeyAt(scm, title + 2)) trigger.titleAt = title + 2;
        }
        return trigger;
    }

    // Finds every mission's trigger, once, and writes what it found.
    void FindTriggers() {
        const std::string& scm = MainScript::Bytes();
        const std::vector<std::size_t> starts = ScriptStarts(scm);
        int found = 0;
        for (Mission& mission : g_missions) {
            mission.trigger = FindTrigger(scm, starts, mission.number);
            if (mission.trigger.end) ++found;
            else Log::Write("Markers: no trigger found for mission " + std::to_string(mission.number));
        }
        Log::Write("Markers: found the triggers of " + std::to_string(found) + " of " + std::to_string(std::size(g_missions)) + " missions");
    }

    // The mission whose trigger this script is, by the position it runs at; nullptr for any other script.
    Mission* MissionOf(const CRunningScript& script) {
        for (Mission& mission : g_missions) {
            if (script.m_nIp >= mission.trigger.start && script.m_nIp < mission.trigger.end) return &mission;
        }
        return nullptr;
    }

    // The text key of the mission's title, from main.scm; empty when its trigger prints none.
    std::string TitleKey(const Mission& mission) {
        return mission.trigger.titleAt ? std::string(MainScript::Bytes().c_str() + mission.trigger.titleAt) : std::string();
    }

    // Test: makes the test marker's trigger launch the other mission under its title, by copying its number and its title's key; done again whenever a new game reloads main.scm.
    void RedirectTestMarker() {
        const Mission* marker = Find(kTestMarker);
        const Mission* launched = Find(kTestLaunched);
        if (!marker || !launched || !marker->trigger.numberAt || CTheScripts::ScriptSpace[marker->trigger.numberAt] == kTestLaunched) return;
        CTheScripts::ScriptSpace[marker->trigger.numberAt] = kTestLaunched;
        if (marker->trigger.titleAt && launched->trigger.titleAt) std::memcpy(CTheScripts::ScriptSpace + marker->trigger.titleAt, MainScript::Bytes().data() + launched->trigger.titleAt, 8);
        Log::Write("Markers: test, the marker of mission " + std::to_string(kTestMarker) + " launches mission " + std::to_string(kTestLaunched) + " under its title " + TitleKey(*launched));
    }

    // A locked mission's trigger is kept asleep, so it never checks whether the player stands on its marker; returns the contacts with an unlocked mission waiting on their marker.
    std::array<bool, RADAR_SPRITE_COUNT> FollowTriggers() {
        std::array<bool, RADAR_SPRITE_COUNT> waiting{};
        for (CRunningScript* script = CTheScripts::pActiveScripts; script; script = script->m_pNext) {
            const Mission* mission = MissionOf(*script);
            if (!mission) continue;
            if (mission->unlocked) waiting[mission->sprite] = true;
            else script->m_nWakeTime = CTimer::m_snTimeInMilliseconds + 1000;
        }
        return waiting;
    }

    // A contact's marker shows on the radar and on the ground only while one of its unlocked missions waits there; the blips of other sprites (save houses) are left alone.
    void ShowWaitingContacts(const std::array<bool, RADAR_SPRITE_COUNT>& waiting) {
        std::array<bool, RADAR_SPRITE_COUNT> managed{};
        for (const Mission& mission : g_missions) managed[mission.sprite] = mission.sprite != RADAR_SPRITE_NONE;
        for (tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (!blip.m_bInUse || blip.m_nBlipType != BLIP_CONTACTPOINT || blip.m_nRadarSprite >= RADAR_SPRITE_COUNT || !managed[blip.m_nRadarSprite]) continue;
            blip.m_nBlipDisplay = waiting[blip.m_nRadarSprite] ? BLIP_DISPLAY_BOTH : BLIP_DISPLAY_NEITHER;
        }
    }

    // The mission a mission's marker launches: the other one for the test marker, else itself.
    const Mission& LaunchedBy(const Mission& mission) {
        const Mission* launched = mission.number == kTestMarker ? Find(kTestLaunched) : nullptr;
        return launched ? *launched : mission;
    }

    // Test: F10 unlocks the next locked mission of the table, shows the title of what its marker launches in the player's language, and writes whether its trigger runs and the title key its trigger holds in memory.
    void UnlockNextOnF10() {
        if (!CPad::NewKeyState.FKeys[9] || CPad::OldKeyState.FKeys[9]) return;
        for (Mission& mission : g_missions) {
            if (mission.unlocked) continue;
            mission.unlocked = true;
            bool running = false;
            for (CRunningScript* script = CTheScripts::pActiveScripts; script; script = script->m_pNext) running = running || MissionOf(*script) == &mission;
            const std::string inMemory = mission.trigger.titleAt ? reinterpret_cast<const char*>(CTheScripts::ScriptSpace + mission.trigger.titleAt) : "";
            Log::Write("Markers: unlocked mission " + std::to_string(mission.number) + ", its marker launches mission " + std::to_string(LaunchedBy(mission).number) + ", its trigger is " + (running ? "running" : "not running") + " and prints the title " + inMemory);
            const std::string key = TitleKey(LaunchedBy(mission));
            if (!key.empty()) CMessages::AddMessageJumpQ(TheText.Get(key.c_str()), 2000, 0);
            return;
        }
    }

    // Runs once per frame: finds the triggers on the first one, then follows the unlocks.
    void Process() {
        static bool triggersFound = false;
        if (!triggersFound) {
            FindTriggers();
            triggersFound = true;
        }
        RedirectTestMarker();
        UnlockNextOnF10();
        ShowWaitingContacts(FollowTriggers());
    }
}

void Markers::Install() {
    plugin::Events::gameProcessEvent += Process;
}
