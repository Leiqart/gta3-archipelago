#include <windows.h>
#include "plugin.h"
#include "CCutsceneMgr.h"
#include "CPad.h"
#include "cDMAudio.h"
#include "common.h"
#include "extensions/KeyCheck.h"
#include "../Log.h"
#include "../Missions.h"
#include "Actions.h"
#include "Autopilot.h"
#include "Cheats.h"
#include "Settings.h"

namespace {
    // With mute=1: the master volumes go to 0 every frame, because the game sets them again from the menu's settings (which stay as saved).
    void Silence() {
        if (!Settings::Mute()) return;
        DMAudio.SetEffectsMasterVolume(0);
        DMAudio.SetMusicMasterVolume(0);
    }

    // Runs on the game thread once per frame: script commands are only safe there, never from our own thread.
    void Process() {
        Silence();
        KeyCheck::Update();
        if (!FindPlayerPed()) return;
        if (KeyCheck::CheckJustDown(VK_F8)) Autopilot::TogglePause();
        // F9 works at any time, even while the game holds the player, to capture a good spot for a mission scenario.
        if (KeyCheck::CheckJustDown(VK_F9)) {
            Actions::LogWhereIAm();
            Actions::Say(L"Cheat: position written to the log");
        }
        Autopilot::Update();
        // The keys do nothing during a cutscene, or while a mission script has taken control of the player.
        if (CCutsceneMgr::ms_running || CPad::GetPad(0)->DisablePlayerControls) return;
        if (KeyCheck::CheckJustDown(VK_F5) && !Actions::GoToMissionBlip()) Actions::Say(L"Cheat: no mission blip");
        if (KeyCheck::CheckJustDown(VK_F6) && Actions::KillMarkedCharacters() == 0) Actions::Say(L"Cheat: no character blip");
        if (KeyCheck::CheckJustDown(VK_F10) && !Missions::ForceMissionFail()) Actions::Say(L"Cheat: no mission to fail");
        if (KeyCheck::CheckJustDown(VK_F7)) {
            if (FindPlayerVehicle()) Actions::Say(L"Cheat: already in a car");
            else Actions::GiveCar();
        }
    }
}

void Cheats::Install(const std::string& folder) {
    Settings::Load(folder + "GTA3Archipelago-dev.ini");
    plugin::Events::gameProcessEvent += Process;
    plugin::Events::menuDrawingEvent += Silence;
    plugin::Events::menuDrawingEvent += Autopilot::UpdateMenu;
    Autopilot::Install();
    Log::Write("Dev build: the autopilot skips the videos, starts the game and plays the test missions (F8 pauses it); cheats on F5 (go to the mission blip), F6 (kill the marked characters), F7 (give a car), F9 (write my position to the log), F10 (fail the mission)");
}
