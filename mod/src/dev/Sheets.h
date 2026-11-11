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
        // For a job without a passed flag (Paramedic): a goal reached once it is done for good, read instead of passedFlag; empty when none.
        Steps::Goal done;
        // The marker the player walks into on foot to start it; none for Give Me Liberty, which the game starts by itself after the intro.
        std::optional<CVector> start;
        // The model of the vehicle the player must sit in at the start, for a side mission like RC Toyz (the Toyz van, 149); 0 to walk in on foot.
        int startModel = 0;
        // True for a job started with the side mission button in its vehicle (Paramedic, Firefighter, Vigilante, Taxi Driver).
        bool startWithButton = false;
        // The island a side mission is played on, which the story opens before it can be played there: 0 Portland, 1 Staunton Island (Last Requests), 2 Shoreside Vale (A Drop in the Ocean).
        int island = 0;
        // Done before walking into the marker, like setting the clock for a mission that only starts at some hours; empty when nothing.
        Steps::Action beforeStart;
        // What the mission changes for good, replayed when the run skips it because later missions rely on it; nullptr when nothing.
        void (*afterSkip)() = nullptr;
        // The steps, in the order the autopilot plays them.
        std::vector<Steps::Step> steps;
    };

    // Every sheet, in story order: the order of the test run.
    const std::vector<Sheet>& Run();
    // True once the sheet's mission is passed for good: its done goal if it has one, else its passed flag or later passed flag.
    bool Done(const Sheet& sheet);
    // The sheet of a mission, by its SCRIPT_NAME, the first not done when several share it (a job played on each island); nullptr for a mission the run does not play, like the intro.
    const Sheet* Find(const std::string& script);
}
