#pragma once
#include <string>

// Dev builds only (build.cmd dev): the autopilot skips the videos, starts the game and plays the test missions, F8 pauses it; F5 goes to the mission blip, F6 kills the marked character, F7 gives a car, F9 logs the position, F3 fails the mission.
namespace Cheats {
    // Reads GTA3Archipelago-dev.ini from the ASI's folder, then runs the keys and the autopilot on the game thread; call it once, from DllMain.
    void Install(const std::string& folder);
}
