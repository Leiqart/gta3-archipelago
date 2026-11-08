#pragma once
#include <functional>
#include <string>

// The game's missions seen from outside: events when one starts, runs, passes or fails, and a way to fail it; everything runs on the game thread.
namespace Missions {
    // Hooks the game loop so that the events fire; call it once, from DllMain, before the game starts.
    void Install();

    // Called when a mission starts, with its SCRIPT_NAME in lowercase as the game stores it ("eight").
    void OnMissionStart(std::function<void(const std::string& name)> callback);
    // Called every frame while a mission runs.
    void OnMissionUpdate(std::function<void(const std::string& name)> callback);
    // Called when the game registers a mission as passed, with the GXT key it registers ("LM1" for Give Me Liberty and Luigi's Girls).
    void OnMissionPassed(std::function<void(const std::string& name, const std::string& key)> callback);
    // Called when a mission ends without being passed: failed, wasted or busted; the intro, which has nothing to pass, is left out.
    void OnMissionFailed(std::function<void(const std::string& name)> callback);

    // The SCRIPT_NAME of the mission running now, empty between missions.
    const std::string& Running();
    // Fails the running mission the way dying does, so it runs its own failure and cleanup; false when it cannot now (cutscene, no player control): try again later.
    bool ForceMissionFail();
    // Skips a mission at its launch (call it from OnMissionStart): its own mission_passed routine runs, none of its setup; false at any other time.
    bool ForceMissionPass();
}
