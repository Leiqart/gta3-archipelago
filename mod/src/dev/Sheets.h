#pragma once
#include <optional>
#include <string>
#include <vector>
#include "CVector.h"
#include "Steps.h"

// Dev builds only: the mission sheets, one per test mission, each a list of steps the autopilot plays one after the other.
namespace Sheets {
    // One test mission and how to play it.
    struct Sheet {
        // Its SCRIPT_NAME in lowercase, as the game stores it (EIGHT in the decoded script runs as "eight").
        const char* script;
        // The script global the game sets to 1 once the mission is passed (g980 is 980).
        int passedFlag;
        // For a race a later mission makes replayable by clearing passedFlag: the flag that later mission sets instead, read too; 0 when none.
        int laterPassedFlag = 0;
        // The marker the player walks into on foot to start it; none for Give Me Liberty, which the game starts by itself after the intro.
        std::optional<CVector> start;
        // The model of the vehicle the player must sit in at the start, for a side mission like RC Toyz (the Toyz van, 149); 0 to walk in on foot.
        int startModel = 0;
        // Done before walking into the marker, like setting the clock for a mission that only starts at some hours; empty when nothing.
        Steps::Action beforeStart;
        // What the mission changes for good, replayed when the run skips it because later missions rely on it; nullptr when nothing.
        void (*afterSkip)() = nullptr;
        // The steps, in the order the autopilot plays them.
        std::vector<Steps::Step> steps;
    };

    // Every sheet, in story order: the order of the test run.
    const std::vector<Sheet>& Run();
    // The sheet of a mission, by its SCRIPT_NAME; nullptr for a mission the run does not play, like the intro.
    const Sheet* Find(const std::string& script);
}
