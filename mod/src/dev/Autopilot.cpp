#include <windows.h>
#include <atomic>
#include <cstring>
#include <map>
#include <string>
#include "plugin.h"
#include "CCamera.h"
#include "CCutsceneMgr.h"
#include "CGame.h"
#include "CMenuManager.h"
#include "CPad.h"
#include "CPools.h"
#include "CTimer.h"
#include "CVehicle.h"
#include "common.h"
#include "../ClientLink.h"
#include "../Log.h"
#include "../Missions.h"
#include "Actions.h"
#include "Autopilot.h"
#include "Recorder.h"
#include "Settings.h"
#include "Sheets.h"

namespace {
    // F8 pauses the autopilot; it plays as soon as a dev build starts, and pauses itself when it is stuck.
    bool g_paused = false;

    // The autopilot cannot go on: with quit_on_stuck it writes why and quits for the automatic test loop, else it pauses and records the player's fix.
    void Stuck(const std::string& why) {
        Log::Write("Autopilot: stuck, " + why);
        if (Settings::QuitOnStuck()) {
            Log::Write("Autopilot: quitting the game (quit_on_stuck)");
            ExitProcess(3);
        }
        g_paused = true;
        Actions::Say(L"Autopilot: stuck, paused (F8 to resume)");
        Recorder::Start("the autopilot is stuck, the player takes over");
    }

    // Game time of the next allowed action: after acting, the autopilot gives the game time to react.
    int g_nextStepTime = 0;
    // The end of the run is announced once.
    bool g_runEndSaid = false;
    // What the autopilot saw last; logged each time it changes, so a stuck run shows where it stops.
    std::string g_lastSeen;
    // The player's car on the previous frame, as a pool handle: unlike a pointer, it is safe to look up once the car is gone.
    int g_lastCarHandle = -1;

    // Each time the player gets out of a car, by hand or not, logs where the car stays: the spot a mission accepted, for its scenario.
    void LogCarOnExit() {
        if (CVehicle* car = FindPlayerVehicle()) {
            g_lastCarHandle = CPools::GetVehicleRef(car);
            return;
        }
        if (g_lastCarHandle < 0) return;
        if (CVehicle* left = CPools::GetVehicle(g_lastCarHandle)) Actions::LogPlaceAndHeading("The player got out of a car, which stays at", *left);
        g_lastCarHandle = -1;
    }

    // The new game is started from the main menu only once.
    bool g_gameStarted = false;
    // Each cutscene is skipped only once.
    bool g_cutsceneSkipped = false;
    // Game time of the last change in what the autopilot sees: outside the sheets, 30 s without one means the run is stuck.
    int g_lastChangeTime = 0;
    // The sheet of the mission skipped last, until it has ended and its lasting changes are replayed; nullptr when none waits.
    const Sheets::Sheet* g_skipped = nullptr;

    // The sheet with this name if it comes before start_at in the run, so the run skips it; nullptr otherwise.
    const Sheets::Sheet* SkippedSheet(const std::string& script) {
        if (Settings::StartAt().empty()) return nullptr;
        for (const Sheets::Sheet& sheet : Sheets::Run()) {
            if (Settings::StartAt() == sheet.script) return nullptr;
            if (script == sheet.script) return &sheet;
        }
        return nullptr;
    }

    // At the launch of a mission before start_at, before it runs anything: forces it to pass, so it leaves nothing behind for the next one.
    void SkipIfBeforeStart(const std::string& name) {
        const Sheets::Sheet* sheet = SkippedSheet(name);
        if (!sheet) return;
        if (!Missions::ForceMissionPass()) {
            Stuck("could not skip " + name + " to start at " + Settings::StartAt());
            return;
        }
        Log::Write("Autopilot: skipped " + name + " to start at " + Settings::StartAt());
        g_skipped = sheet;
    }

    bool StartNextMission();

    // Once a skipped mission has ended: replays what it changes for good, then gives back the control and camera its trigger may have taken.
    void FinishSkip() {
        if (!g_skipped || !Missions::Running().empty()) return;
        if (g_skipped->afterSkip) g_skipped->afterSkip();
        Actions::GiveControlBack();
        // Its passed routine opened the contact's next missions on this marker, and their triggers wait 250 ms (g20) before looking: going on to the run's next marker at once keeps Luigi 4 from starting after a skipped Drive Misty For Me.
        StartNextMission();
        g_skipped = nullptr;
    }

    // gGameState values of gta3.exe's start-up, as in re3 (plugin-sdk only gives the variable): the logo video, the titles video, the one-time loading.
    constexpr int kLogoVideo = 2, kTitlesVideo = 4, kInitOnce = 5;

    // Wall-clock time of the last game frame the autopilot saw, written by the game thread and read by the watch thread; 0 before the first one.
    std::atomic<unsigned long long> g_lastFrameTick{0};

    // The game's main window: the visible top-level window of this process.
    HWND GameWindow() {
        HWND found = nullptr;
        EnumWindows([](HWND window, LPARAM result) -> BOOL {
            DWORD process = 0;
            GetWindowThreadProcessId(window, &process);
            if (process != GetCurrentProcessId() || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
            *reinterpret_cast<HWND*>(result) = window;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&found));
        return found;
    }

    // The game window's own message handler, still called for every message the autopilot lets through.
    WNDPROC g_originalWindowProc = nullptr;

    // Runs before the game's handler: GTA III stops its loop once Windows tells it that it lost the focus, so in unattended runs it is never told.
    LRESULT CALLBACK BackgroundWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if ((message == WM_ACTIVATEAPP && !wParam) || (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE)) return 0;
        return CallWindowProcA(g_originalWindowProc, window, message, wParam, lParam);
    }

    // Puts BackgroundWindowProc in front of the game's handler, once the game has created its window; true once done.
    bool RunInBackground() {
        HWND window = GameWindow();
        if (!window) return false;
        g_originalWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&BackgroundWindowProc)));
        Log::Write(g_originalWindowProc ? "Autopilot: the game keeps running without the focus" : "Autopilot: could not keep the game running without the focus");
        return true;
    }

    // Our own thread, as the start-up runs before any game event: skips the videos like a key press, then watches the session once a second.
    DWORD WINAPI WatchThread(void*) {
        bool background = !Settings::RunInBackground();
        while (gGameState < kInitOnce) {
            if (!background) background = RunInBackground();
            const int state = gGameState;
            if (state == kLogoVideo || state == kTitlesVideo) {
                gGameState = state + 1;
                Log::Write("Autopilot: skipped a start-up video");
            }
            Sleep(50);
        }
        for (;;) {
            Sleep(1000);
            if (!background) background = RunInBackground();
            // No game frame for a minute once the game has started: it froze, or a menu holds it.
            const unsigned long long lastFrame = g_lastFrameTick;
            if (Settings::QuitOnStuck() && lastFrame != 0 && GetTickCount64() - lastFrame > 60000) {
                Log::Write("Autopilot: stuck, no game frame for 60 s (frozen, or held by a menu)");
                Log::Write("Autopilot: quitting the game (quit_on_stuck)");
                ExitProcess(4);
            }
        }
    }

    // Skips a playing cutscene once, only when the game itself lets a player do it: loaded, its flyby camera running, never the ending; one second in.
    void SkipCutscene() {
        if (!CCutsceneMgr::ms_running) {
            g_cutsceneSkipped = false;
            return;
        }
        const CCam& camera = TheCamera.m_asCams[TheCamera.m_nActiveCam];
        if (g_cutsceneSkipped || CCutsceneMgr::ms_cutsceneLoadStatus != 0 || camera.m_nCamMode != MODE_FLYBY) return;
        if (_stricmp(CCutsceneMgr::ms_cutsceneName, "end") == 0 || CCutsceneMgr::GetCutsceneTimeInMilleseconds() < 1000) return;
        CCutsceneMgr::FinishCutscene();
        g_cutsceneSkipped = true;
        Log::Write("Autopilot: skipped a cutscene");
    }

    // One line with everything the autopilot decides on: the running mission, a cutscene, the control flags, the blip, the car.
    std::string WhatISee() {
        const char* running = Actions::RunningMission();
        std::string seen = std::string("mission ") + (running ? running : "none");
        seen += CCutsceneMgr::ms_running ? ", cutscene" : "";
        seen += ", controls disabled flags " + std::to_string(CPad::GetPad(0)->DisablePlayerControls);
        const tRadarTrace* blip = Actions::MissionBlip(false);
        static const char* const kBlipNames[] = {"none", "car", "char", "object", "coord"};
        seen += std::string(", blip ") + (blip && blip->m_nBlipType <= BLIP_COORD ? kBlipNames[blip->m_nBlipType] : "none");
        seen += FindPlayerVehicle() ? ", in a car" : ", on foot";
        return seen;
    }

    // Between missions: walks into the marker of the next sheet not passed yet; returns true when it acted.
    bool StartNextMission() {
        for (const Sheets::Sheet& sheet : Sheets::Run()) {
            if (Actions::ScriptGlobal(sheet.passedFlag) == 1) continue;
            if (!sheet.start) return false;
            if (sheet.beforeStart) sheet.beforeStart();
            if (FindPlayerVehicle()) Actions::LeaveCarTo(*sheet.start);
            else if (Actions::Distance2D(Actions::PlayerPosition(), *sheet.start) > 1.0f) Actions::TeleportTo(*sheet.start);
            else return false;
            return true;
        }
        if (!g_runEndSaid) {
            Actions::Say(L"Autopilot: all test missions are passed");
            Log::Write("Autopilot: all test missions are passed");
            g_runEndSaid = true;
            if (Settings::QuitWhenDone()) {
                Log::Write("Autopilot: quitting the game (quit_when_done)");
                ExitProcess(0);
            }
        }
        return false;
    }

    // The sheet the autopilot plays now, the index of its current step, and the game times the sheet and the step began.
    const Sheets::Sheet* g_sheet = nullptr;
    size_t g_step = 0;
    int g_sheetStartTime = 0;
    int g_stepStartTime = 0;

    // A duration in game milliseconds as seconds with one decimal, like "4.2 s".
    std::string Seconds(int milliseconds) {
        return std::to_string(milliseconds / 1000) + "." + std::to_string(milliseconds % 1000 / 100) + " s";
    }

    // The current step for the log, like "luigi2 step 3/6 (respray the car at the Pay 'n' Spray)".
    std::string StepLabel() {
        return std::string(g_sheet->script) + " step " + std::to_string(g_step + 1) + "/" + std::to_string(g_sheet->steps.size()) + " (" + g_sheet->steps[g_step].name + ")";
    }

    // Starts the clock of the current step and writes it to the log.
    void BeginStep() {
        g_stepStartTime = CTimer::m_snTimeInMilliseconds;
        Log::Write("Autopilot: " + StepLabel());
    }

    // Follows the running mission: its sheet starts at the first step, so a failed mission replays from the start; none between missions.
    void FollowSheet(const Sheets::Sheet* sheet) {
        if (sheet == g_sheet) return;
        const int now = CTimer::m_snTimeInMilliseconds;
        if (g_sheet) Log::Write("Autopilot: the " + std::string(g_sheet->script) + " sheet ended after " + Seconds(now - g_sheetStartTime));
        g_sheet = sheet;
        g_step = 0;
        g_sheetStartTime = now;
        g_lastChangeTime = now;
        if (!sheet) return;
        Log::Write("Autopilot: playing the " + std::string(sheet->script) + " sheet, " + std::to_string(sheet->steps.size()) + " steps");
        BeginStep();
    }

    // The test loop's rule: inside a sheet, each step must be done in its time; outside, what the autopilot sees must change within 30 s.
    bool CheckStuck(const std::string& seen, int now) {
        if (g_sheet && g_step < g_sheet->steps.size()) {
            const int timeout = g_sheet->steps[g_step].timeoutSeconds;
            if (now - g_stepStartTime <= timeout * 1000) return false;
            g_stepStartTime = now;
            Actions::LogWhereIAm();
            Stuck(StepLabel() + " not done after " + std::to_string(timeout) + " s, the autopilot sees: " + seen);
            return true;
        }
        if (now - g_lastChangeTime <= 30000) return false;
        g_lastChangeTime = now;
        Actions::LogWhereIAm();
        Stuck("nothing changed for 30 s, still: " + seen);
        return true;
    }

    // Plays the current sheet: passes every step whose goal is reached, then does the current step's action, at most once every 2 s.
    void PlaySheet(int now) {
        const std::vector<Steps::Step>& steps = g_sheet->steps;
        // Several steps at once when the mission moved faster than the autopilot.
        while (g_step < steps.size() && steps[g_step].goal && steps[g_step].goal()) {
            Log::Write("Autopilot: " + StepLabel() + " done in " + Seconds(now - g_stepStartTime));
            if (++g_step < steps.size()) BeginStep();
        }
        if (g_step >= steps.size() || now < g_nextStepTime || !steps[g_step].action) return;
        steps[g_step].action();
        // Two seconds after each action, so the mission script sees the change before the next one.
        g_nextStepTime = now + 2000;
    }
}

void Autopilot::Update() {
    g_lastFrameTick = GetTickCount64();
    Recorder::Update();
    const int now = CTimer::m_snTimeInMilliseconds;
    const std::string seen = WhatISee();
    if (seen != g_lastSeen) {
        Log::Write("Autopilot sees: " + seen);
        g_lastSeen = seen;
        g_lastChangeTime = now;
    }
    LogCarOnExit();
    // A mission the run skips is not played, even for the frames it takes to run its passed routine.
    const char* running = Actions::RunningMission();
    FollowSheet(running && !g_skipped ? Sheets::Find(running) : nullptr);
    if (g_paused) {
        // The clocks stop while the player plays by hand.
        g_lastChangeTime = now;
        g_stepStartTime = now;
        return;
    }
    if (CheckStuck(seen, now)) return;
    FinishSkip();
    SkipCutscene();
    // No action during a cutscene, or while a mission script has taken control of the player, and 1.5 s more once it gives it back.
    if (CCutsceneMgr::ms_running || CPad::GetPad(0)->DisablePlayerControls) {
        g_nextStepTime = now + 1500;
        return;
    }
    if (g_sheet) PlaySheet(now);
    else if (!running && now >= g_nextStepTime && StartNextMission()) g_nextStepTime = now + 2000;
}

namespace {
    // How many times in a row each mission of the run has failed: a mission that keeps failing replays forever without ever being stuck.
    std::map<std::string, int> g_failuresInARow;

    // A failed mission replays from its first step; the second failure in a row stops the run, with the step it failed at.
    void OnRunMissionFailed(const std::string& name) {
        if (!Sheets::Find(name)) return;
        const std::string at = g_sheet && name == g_sheet->script && g_step < g_sheet->steps.size() ? " at " + StepLabel() : "";
        Log::Write("Autopilot: " + name + " failed" + at);
        if (++g_failuresInARow[name] >= 2) {
            g_failuresInARow[name] = 0;
            Actions::LogWhereIAm();
            Stuck(name + " failed twice in a row" + at);
        }
    }
}

void Autopilot::Install() {
    // A test mission before start_at is forced to pass at its launch, the only moment it holds nothing yet.
    Missions::OnMissionStart(SkipIfBeforeStart);
    Missions::OnMissionFailed(OnRunMissionFailed);
    Missions::OnMissionPassed([](const std::string& name, const std::string&) { g_failuresInARow[name] = 0; });
    CreateThread(nullptr, 0, WatchThread, nullptr, 0, nullptr);
}

void Autopilot::UpdateMenu() {
    // Unattended runs (run_in_background): a pause menu opened anyway would hold the run, so resume like the Resume button does.
    if (Settings::RunInBackground() && !g_paused && !FrontEndMenuManager.m_bGameNotLoaded && FrontEndMenuManager.m_bMenuActive) {
        CMenuManager::RequestFrontEndShutDown();
        Log::Write("Autopilot: closed the pause menu to resume the run");
        return;
    }
    // Once, on the main menu of a fresh start, with the room connected: the same function the Run Archipelago button calls.
    if (g_paused || g_gameStarted || !ClientLink::IsConnected()) return;
    if (!FrontEndMenuManager.m_bGameNotLoaded || FrontEndMenuManager.m_nCurrentMenuPage != MENUPAGE_START_MENU) return;
    g_gameStarted = true;
    FrontEndMenuManager.DoSettingsBeforeStartingAGame();
    Log::Write("Autopilot: started a new game from the main menu");
}

void Autopilot::TogglePause() {
    g_paused = !g_paused;
    Actions::Say(g_paused ? L"Autopilot: paused (F8 to resume)" : L"Autopilot: playing");
    Log::Write(g_paused ? "Autopilot paused" : "Autopilot playing");
    // While paused the player plays by hand: record it, to turn the correction into scenario steps.
    if (g_paused) Recorder::Start("paused with F8, the player takes over");
    else Recorder::Stop();
}
