#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "plugin.h"
#include "CPlayerPed.h"
#include "CRadar.h"
#include "CRunningScript.h"
#include "CTheScripts.h"
#include "CTimer.h"
#include "common.h"
#include "eScriptCommands.h"
#include "Log.h"
#include "MainScript.h"
#include "Markers.h"

namespace {
    // The type byte main.scm writes before an argument's value: a 32-bit number, a global variable, an 8-bit number, a coordinate (16 bits, in sixteenths).
    constexpr char kInt32 = 1, kGlobal = 2, kInt8 = 4, kCoordinate = 6;

    // A command as main.scm stores it: its opcode in 2 bytes, low byte first, then its arguments.
    std::string Command(eScriptCommands opcode, const std::string& arguments = "") {
        return std::string{static_cast<char>(opcode & 0xFF), static_cast<char>(opcode >> 8)} + arguments;
    }

    // The player as the triggers name him: global 528, where main.scm keeps his handle.
    const std::string kPlayer = {kGlobal, '\x10', '\x02'};

    // The commands a trigger is found by: the script starts, the launch of its mission, its title, and the LOCATE of the player on its marker (on foot or any means, stopped or not).
    const std::string kStartNewScript = Command(COMMAND_START_NEW_SCRIPT, {kInt32});
    const std::string kLaunchMission = Command(COMMAND_LOAD_AND_LAUNCH_MISSION_INTERNAL, {kInt8});
    const std::string kPrintBig = Command(COMMAND_PRINT_BIG);
    const std::string kLocates[] = {
        Command(COMMAND_LOCATE_PLAYER_ON_FOOT_3D, kPlayer),
        Command(COMMAND_LOCATE_STOPPED_PLAYER_ON_FOOT_3D, kPlayer),
        Command(COMMAND_LOCATE_PLAYER_ANY_MEANS_3D, kPlayer),
        Command(COMMAND_LOCATE_STOPPED_PLAYER_ANY_MEANS_3D, kPlayer),
    };

    // What START_NEW_SCRIPT runs, CTheScripts::StartNewScript at 0x439000 in gta3.exe 1.0 (plugin-sdk does not declare it): starts a script at this position of ScriptSpace.
    CRunningScript* StartNewScript(unsigned int ip) {
        return plugin::CallAndReturn<CRunningScript*, 0x439000, unsigned int>(ip);
    }

    // How long a trigger is put to sleep at a time; pushed back every frame, it never wakes until let go.
    constexpr unsigned int kTriggerSleep = 1000;
    // How close, in metres, a contact blip must be to a marker to be the one marking it.
    constexpr float kSamePlace = 5.0f;

    // A mission's trigger: the script waiting for the player on its marker, from its first byte to the next script's.
    struct Trigger {
        std::size_t start = 0;
        std::size_t end = 0;
        // Where the text key of its title is in main.scm; 0 when it prints none.
        std::size_t titleAt = 0;
        // Where its marker is: the place its LOCATE checks the player against.
        CVector marker;
    };

    // A mission started from a contact's marker or phone: its number in main.scm, the contact's radar sprite (none for Marty's phone, which has no blip), and its state.
    struct Mission {
        unsigned char number;
        unsigned short sprite;
        // Unlocked by an item of the room, for every game played with it.
        bool unlocked = false;
        Trigger trigger;
        // The rest is about the game being played, forgotten when another starts or is loaded.
        // Whether its trigger ran on the last frame.
        bool running = false;
        // Whether this mod has started its trigger: once, after which the trigger waits on its marker or ends by itself (mission passed).
        bool started = false;
        // The contact blip this mod put on its marker, as CRadar's handle: the slot in the low 16 bits, that slot's use count (at least 1, so never 0) in the high ones; 0 when none.
        int blip = 0;
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

    // The mission of the table with this number; nullptr if it is not in it.
    Mission* Find(unsigned char number) {
        for (Mission& mission : g_missions) {
            if (mission.number == number) return &mission;
        }
        return nullptr;
    }

    // The mission whose trigger this script is, by the position it runs at; nullptr for any other script.
    Mission* MissionOf(const CRunningScript& script) {
        for (Mission& mission : g_missions) {
            if (script.m_nIp >= mission.trigger.start && script.m_nIp < mission.trigger.end) return &mission;
        }
        return nullptr;
    }

    // The mission's name in the log: its number and the text key of its title, like "28 (JM5)".
    std::string Name(const Mission& mission) {
        const char* title = mission.trigger.titleAt ? MainScript::Bytes().c_str() + mission.trigger.titleAt : "";
        return std::to_string(mission.number) + " (" + title + ")";
    }

    // Every position where a START_NEW_SCRIPT starts a script in the main part (where the triggers live), sorted; most triggers are started from the missions' code.
    std::vector<std::size_t> ScriptStarts(const std::string& scm) {
        std::vector<std::size_t> starts;
        for (std::size_t at = scm.find(kStartNewScript); at != std::string::npos && at + kStartNewScript.size() + 4 <= scm.size(); at = scm.find(kStartNewScript, at + 1)) {
            const int position = *reinterpret_cast<const int*>(scm.data() + at + kStartNewScript.size());
            if (position > 0 && static_cast<std::size_t>(position) < MainScript::MainSize()) starts.push_back(static_cast<std::size_t>(position));
        }
        std::sort(starts.begin(), starts.end());
        return starts;
    }

    // Reads the coordinate argument at this position of main.scm into value; false when another type of argument is there.
    bool ReadCoordinate(const std::string& scm, std::size_t at, float& value) {
        if (at + 3 > scm.size() || scm[at] != kCoordinate) return false;
        value = *reinterpret_cast<const short*>(scm.data() + at + 1) / 16.0f;
        return true;
    }

    // The place of a trigger's marker: the x, y and z of its first LOCATE of the player, between start and end; all zero when there is none.
    CVector FindMarker(const std::string& scm, std::size_t start, std::size_t end) {
        std::size_t first = std::string::npos;
        for (const std::string& locate : kLocates) first = std::min(first, scm.find(locate, start));
        if (first >= end) return {};
        // All four LOCATE patterns have the same size; each coordinate after them takes 3 bytes.
        const std::size_t at = first + kLocates[0].size();
        CVector marker;
        if (!ReadCoordinate(scm, at, marker.x) || !ReadCoordinate(scm, at + 3, marker.y) || !ReadCoordinate(scm, at + 6, marker.z)) return {};
        return marker;
    }

    // A mission's trigger: the script holding its LOAD_AND_LAUNCH_MISSION_INTERNAL in the main part, between the script starts around it; all zero when not found.
    Trigger FindTrigger(const std::string& scm, const std::vector<std::size_t>& starts, unsigned char number) {
        const std::size_t at = scm.find(kLaunchMission + static_cast<char>(number));
        if (at == std::string::npos || at >= MainScript::MainSize()) return {};
        const auto next = std::upper_bound(starts.begin(), starts.end(), at);
        if (next == starts.begin()) return {};
        Trigger trigger;
        trigger.start = *(next - 1);
        trigger.end = next != starts.end() ? *next : MainScript::MainSize();
        // Its title is the last PRINT_BIG before the launch, an 8-byte text key the game looks up in the player's language.
        for (std::size_t title = scm.find(kPrintBig, trigger.start); title < at; title = scm.find(kPrintBig, title + 1)) {
            if (MainScript::IsKeyAt(scm, title + kPrintBig.size())) trigger.titleAt = title + kPrintBig.size();
        }
        trigger.marker = FindMarker(scm, trigger.start, at);
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

    // True while UNDRESS_CHAR has taken the player's model away, until DRESS_CHAR (the intro and Give Me Liberty change his clothes so).
    bool PlayerUndressed() {
        const CPlayerPed* player = FindPlayerPed();
        return player && !player->m_pRwObject;
    }

    // Notes which triggers run, and keeps asleep every script of a locked mission's trigger (two once the story starts it again), so none checks whether the player stands on the marker.
    void FollowTriggers() {
        // An undressed player has no animations, and a trigger's LOCATE_STOPPED reads them: started out of order, a trigger may run then, so all sleep until he is dressed.
        const bool undressed = PlayerUndressed();
        for (Mission& mission : g_missions) mission.running = false;
        for (CRunningScript* script = CTheScripts::pActiveScripts; script; script = script->m_pNext) {
            Mission* mission = MissionOf(*script);
            if (!mission) continue;
            mission->running = true;
            if (!mission->unlocked || undressed) script->m_nWakeTime = CTimer::m_snTimeInMilliseconds + kTriggerSleep;
        }
    }

    // Any order: an unlocked mission whose trigger does not run gets it started, once a game, as the mission whose pass starts it in the story may not be passed yet.
    void StartMissingTriggers() {
        for (Mission& mission : g_missions) {
            if (!mission.unlocked || mission.running || mission.started || !mission.trigger.end) continue;
            mission.started = true;
            StartNewScript(static_cast<unsigned int>(mission.trigger.start));
            Log::Write("Markers: started the trigger of mission " + Name(mission) + " at " + std::to_string(mission.trigger.start));
        }
    }

    // True when a contact's blip already marks this place, wherever the game or this mod made it.
    bool HasContactBlipAt(const CVector& place) {
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (blip.m_bInUse && blip.m_nBlipType == BLIP_CONTACTPOINT && (blip.m_vecPos - place).Magnitude2D() < kSamePlace) return true;
        }
        return false;
    }

    // Any order: a waiting mission whose contact has no blip on its marker (Kenji's comes from an Asuka mission) gets one, on the radar and on the ground, until its trigger ends.
    // A passed mission's trigger runs up to 250 ms more (WAIT g20) before it sees the pass and ends: a blip it gets then goes with it.
    void FollowContactBlips() {
        for (Mission& mission : g_missions) {
            if (mission.blip && !mission.running) {
                CRadar::ClearBlip(mission.blip);
                mission.blip = 0;
                Log::Write("Markers: removed the blip of mission " + Name(mission));
            }
            if (mission.blip || !mission.unlocked || !mission.running || mission.sprite == RADAR_SPRITE_NONE || HasContactBlipAt(mission.trigger.marker)) continue;
            // What ADD_SPRITE_BLIP_FOR_CONTACT_POINT does: a contact blip at the place, then its sprite.
            mission.blip = CRadar::SetCoordBlip(BLIP_CONTACTPOINT, mission.trigger.marker, 2, BLIP_DISPLAY_BOTH);
            CRadar::SetBlipSprite(mission.blip, mission.sprite);
            Log::Write("Markers: added a blip for mission " + Name(mission));
        }
    }

    // A contact's marker shows on the radar and on the ground only while one of its unlocked missions waits there; the blips of other sprites (save houses) are left alone.
    void ShowWaitingContacts() {
        std::array<bool, RADAR_SPRITE_COUNT> managed{}, waiting{};
        for (const Mission& mission : g_missions) {
            managed[mission.sprite] = mission.sprite != RADAR_SPRITE_NONE;
            if (mission.unlocked && mission.running) waiting[mission.sprite] = true;
        }
        for (tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (!blip.m_bInUse || blip.m_nBlipType != BLIP_CONTACTPOINT || blip.m_nRadarSprite >= RADAR_SPRITE_COUNT || !managed[blip.m_nRadarSprite]) continue;
            blip.m_nBlipDisplay = waiting[blip.m_nRadarSprite] ? BLIP_DISPLAY_BOTH : BLIP_DISPLAY_NEITHER;
        }
    }

    // A new game, or a loaded one, starts with the scripts and blips of its own: what this mod started or added before belongs to another game.
    void ForgetGame() {
        for (Mission& mission : g_missions) {
            mission.running = false;
            mission.started = false;
            mission.blip = 0;
        }
    }

    // Runs once per frame: finds the triggers on the first one, then follows the unlocks.
    void Process() {
        static bool triggersFound = false;
        if (!triggersFound) {
            FindTriggers();
            triggersFound = true;
        }
        FollowTriggers();
        StartMissingTriggers();
        FollowContactBlips();
        ShowWaitingContacts();
    }
}

void Markers::Install() {
    // initScriptsEvent follows CTheScripts::Init, which a new game, a loaded one and a restart all run.
    plugin::Events::initScriptsEvent += ForgetGame;
    plugin::Events::gameProcessEvent += Process;
}

void Markers::Unlock(unsigned char number) {
    Mission* mission = Find(number);
    if (!mission) {
        Log::Write("Markers: mission " + std::to_string(number) + " is not in the table, nothing to unlock");
        return;
    }
    mission->unlocked = true;
    Log::Write("Markers: unlocked mission " + Name(*mission));
}

bool Markers::IsPlayable(unsigned char number) {
    const Mission* mission = Find(number);
    return !mission || (mission->unlocked && mission->running);
}
