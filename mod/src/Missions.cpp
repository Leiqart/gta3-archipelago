#include <windows.h>
#include <cctype>
#include <cstring>
#include <vector>
#include "plugin.h"
#include "CCutsceneMgr.h"
#include "CPad.h"
#include "CRunningScript.h"
#include "CStats.h"
#include "CTheScripts.h"
#include "Log.h"
#include "MainScript.h"
#include "Missions.h"

namespace {
    // The functions registered for each event, called in the order they were registered.
    std::vector<std::function<void(const std::string&)>> g_onStart;
    std::vector<std::function<void(const std::string&)>> g_onUpdate;
    std::vector<std::function<void(const std::string&, const std::string&)>> g_onPassed;
    std::vector<std::function<void(const std::string&)>> g_onFailed;

    // The mission running now, and what the game's pass counters said when it started.
    std::string g_running;
    int g_passedCountAtStart = 0;
    std::string g_passedNameAtStart;
    bool g_passed = false;

    // The script of the running mission: LOAD_AND_LAUNCH_MISSION marks it m_bIsMission; nullptr between missions.
    CRunningScript* MissionScript() {
        for (CRunningScript* script = CTheScripts::pActiveScripts; script; script = script->m_pNext) {
            if (script->m_bIsMission) return script;
        }
        return nullptr;
    }

    // LOAD_AND_LAUNCH_MISSION copies a mission to ScriptSpace[SIZE_MAIN_SCRIPT]: its jumps count from there.
    constexpr unsigned int kMissionBase = 131072;

    // The running mission's code, found by matching the start of its slot in ScriptSpace; behind it in memory lie the next missions of the file.
    const std::string* RunningMissionCode() {
        const std::vector<std::string>& missions = MainScript::Missions();
        const char* slot = reinterpret_cast<const char*>(CTheScripts::ScriptSpace + kMissionBase);
        for (const std::string& code : missions) {
            if (code.size() >= 64 && std::memcmp(slot, code.data(), 64) == 0) return &code;
        }
        return nullptr;
    }

    // A just-launched mission is still called "noname": its name is read from its code's first SCRIPT_NAME (opcode 0x03A4 and the name), in lowercase as the game stores it.
    // Most missions start with it, but some name themselves later: JOEY1 starts with GOSUB and runs SCRIPT_NAME at byte 59, too late to be skipped at launch.
    std::string NameFromCode() {
        const unsigned char* slot = CTheScripts::ScriptSpace + kMissionBase;
        if (*reinterpret_cast<const unsigned short*>(slot) == 0x03A4) {
            std::string name;
            for (int i = 2; i < 10 && slot[i]; ++i) name += static_cast<char>(std::tolower(slot[i]));
            return name;
        }
        const std::string* code = RunningMissionCode();
        return code ? MainScript::ScriptName(*code) : "";
    }

    // The mission's name, from the moment LOAD_AND_LAUNCH_MISSION starts it; empty while there is none.
    std::string MissionName() {
        CRunningScript* script = MissionScript();
        if (!script) return "";
        if (std::strcmp(script->m_szName, "noname") != 0) return script->m_szName;
        return NameFromCode();
    }

    // Offset of the running mission's mission_passed routine: right after the RETURN (0x0051) before its REGISTER_MISSION_PASSED (0x0318 and a key); -1 if none.
    // Checked against the full decoding of main.scm: it finds the same routine for all 73 missions that have one, and nothing in the others.
    int PassedRoutine() {
        const std::string* code = RunningMissionCode();
        if (!code) return -1;
        for (std::size_t at = code->find("\x18\x03", 0, 2); at != std::string::npos; at = code->find("\x18\x03", at + 1, 2)) {
            if (!MainScript::IsKeyAt(*code, at + 2)) continue;
            const std::size_t ret = code->rfind("\x51\x00", at, 2);
            return ret == std::string::npos ? -1 : static_cast<int>(ret + 2);
        }
        return -1;
    }

    // A 16-bit value of a mission's code, 0 past its end.
    unsigned short Read16(const std::string& code, std::size_t at) {
        return at + 2 <= code.size() ? *reinterpret_cast<const unsigned short*>(code.data() + at) : 0;
    }

    // True for a mission script LOAD_AND_LAUNCH_MISSION has just started: it has not run its first command, so it holds nothing yet.
    bool NotStartedYet(const CRunningScript& script) {
        return script.m_nIp == kMissionBase && script.m_nSP == 0;
    }

    // Offset right after the mission's first command GOSUB mission_start (opcode 0x0050), after an optional SCRIPT_NAME (0x03A4, 8-byte name); -1 if not so.
    int FirstGosubReturn(const std::string& code) {
        std::size_t at = Read16(code, 0) == 0x03A4 ? 10 : 0;
        if (Read16(code, at) != 0x0050 || at + 3 > code.size()) return -1;
        // The jump target's type byte tells its size: 1 is a 32-bit number, 4 an 8-bit one, 5 a 16-bit one.
        const unsigned char type = static_cast<unsigned char>(code[at + 2]);
        const int size = type == 1 ? 4 : type == 4 ? 1 : type == 5 ? 2 : 0;
        return size ? static_cast<int>(at + 3 + size) : -1;
    }

    // Force a mission to fail only while the player plays it: its setup holds a cutscene or special characters it frees later, and the next mission loading into those slots hangs the game.
    bool PlayerIsPlaying() {
        return !CCutsceneMgr::ms_loaded && !CCutsceneMgr::ms_running && CPad::GetPad(0)->DisablePlayerControls == 0;
    }

    // REGISTER_MISSION_PASSED writes the key in LastMissionPassedName and counts the pass: either change means the mission was passed.
    bool PassRegisteredSinceStart() {
        return CStats::MissionsPassed != g_passedCountAtStart || g_passedNameAtStart != CStats::LastMissionPassedName;
    }

    void StartMission(const std::string& name) {
        g_running = name;
        g_passedCountAtStart = CStats::MissionsPassed;
        g_passedNameAtStart = CStats::LastMissionPassedName;
        g_passed = false;
        Log::Write("Mission started: " + name);
        for (auto& callback : g_onStart) callback(name);
    }

    void EndMission() {
        const std::string name = g_running;
        g_running.clear();
        if (g_passed || name == "intro") return;
        Log::Write("Mission failed: " + name);
        for (auto& callback : g_onFailed) callback(name);
    }

    // Fires the passed event once, as soon as the game has registered the pass.
    void CheckPassed() {
        if (g_running.empty() || g_passed || !PassRegisteredSinceStart()) return;
        g_passed = true;
        const std::string key = CStats::LastMissionPassedName;
        Log::Write("Mission passed: " + g_running + " (" + key + ")");
        for (auto& callback : g_onPassed) callback(g_running, key);
    }

    // Once per frame on the game thread, after the scripts have run: compares the running mission with the previous frame.
    void Process() {
        // A mission registers its pass and terminates in the same frame, so look for the pass before deciding that it ended without one.
        CheckPassed();
        const std::string name = MissionName();
        if (!g_running.empty() && name != g_running) EndMission();
        if (g_running.empty() && !name.empty()) StartMission(name);
        if (g_running.empty()) return;
        CheckPassed();
        for (auto& callback : g_onUpdate) callback(g_running);
    }
}

void Missions::Install() {
    plugin::Events::gameProcessEvent += Process;
}

void Missions::OnMissionStart(std::function<void(const std::string&)> callback) { g_onStart.push_back(std::move(callback)); }
void Missions::OnMissionUpdate(std::function<void(const std::string&)> callback) { g_onUpdate.push_back(std::move(callback)); }
void Missions::OnMissionPassed(std::function<void(const std::string&, const std::string&)> callback) { g_onPassed.push_back(std::move(callback)); }
void Missions::OnMissionFailed(std::function<void(const std::string&)> callback) { g_onFailed.push_back(std::move(callback)); }

const std::string& Missions::Running() {
    return g_running;
}

bool Missions::ForceMissionFail() {
    CRunningScript* script = MissionScript();
    // Every mission runs as "gosub mission_start" then "if wasted or busted, gosub mission_failed": it must be inside the first gosub.
    if (!script || script->m_nSP == 0 || !PlayerIsPlaying()) return false;
    // What the game itself does when the player dies or is busted (deatharrest): back to that first return address, marked wasted or busted.
    script->m_nIp = script->m_anStack[0];
    script->m_nSP = 0;
    script->m_nWakeTime = 0;
    script->m_bWastedOrBusted = true;
    *reinterpret_cast<int*>(CTheScripts::ScriptSpace + CTheScripts::OnAMissionFlag) = 0;
    Log::Write(std::string("Mission forced to fail: ") + script->m_szName);
    return true;
}

bool Missions::ForceMissionPass() {
    CRunningScript* script = MissionScript();
    // Only before its first command: midway, it holds what its setup loaded or created (8-Ball in special slot 1), and the next mission crashes on it.
    if (!script) return false;
    if (!NotStartedYet(*script)) {
        Log::Write("Cannot force " + MissionName() + " to pass: it already runs, at byte " + std::to_string(static_cast<int>(script->m_nIp) - static_cast<int>(kMissionBase)) + " with " + std::to_string(script->m_nSP) + " GOSUB(s) open");
        return false;
    }
    const std::string* code = RunningMissionCode();
    const int routine = PassedRoutine();
    const int topReturn = code ? FirstGosubReturn(*code) : -1;
    const std::string name = MissionName();
    if (routine < 0 || topReturn < 0) {
        Log::Write("Cannot force " + name + " to pass: its mission_passed routine or its first GOSUB was not found");
        return false;
    }
    // As if "gosub mission_start" had run: the passed routine's RETURN goes back to the top, which then runs the cleanup and terminates.
    strncpy_s(script->m_szName, name.c_str(), _TRUNCATE);
    script->m_anStack[0] = kMissionBase + topReturn;
    script->m_nSP = 1;
    script->m_nIp = kMissionBase + routine;
    script->m_nWakeTime = 0;
    script->m_bWastedOrBusted = false;
    Log::Write("Mission forced to pass: " + name + ", from its mission_passed routine at " + std::to_string(routine));
    return true;
}
