#pragma once

namespace MissionManager {
    // Type used to identify a GTA III mission.
    using MissionId = int;

    // Identifier used when no mission launch was observed.
    constexpr MissionId kUnknownMission = -1;

    // Frames without a mission script before the mission counts as over.
    constexpr int kEndConfirmationFrames = 5;

    // Event produced by observing the current game state.
    enum class Event {
        None,
        Enter,
        Stay,
        Success,
        Failed,
    };

    // Receiver of Enter, Success and Failed events (never Stay).
    using EventSink = void (*)(Event event, MissionId missionId);

    // Plugs in the event receiver; nullptr removes it.
    void SetEventSink(EventSink sink);

    // Clears the internal state, for example when a new game is loaded.
    void Reset();

    // Remembers the identifier read in LOAD_AND_LAUNCH_MISSION_INTERNAL (0x0417).
    void NotifyMissionLaunch(MissionId missionId);
    // Verified script checkpoint, used when a split mission has no vanilla pass counter.
    // Still waits for real script cleanup; never changes the game's counters.
    bool ConfirmScriptedSuccess(MissionId missionId);

    // Compares a game snapshot with the previous one and calls the matching OnMission.
    Event Update(bool missionScriptRunning, int missionsPassed);

    // Called once when the player enters a mission.
    void OnMissionEnter(MissionId missionId);

    // Called once when the player completes a mission successfully.
    void OnMissionSuccess(MissionId missionId);

    // Called on every update while the mission is running.
    void OnMissionStay(MissionId missionId);

    // Called once when the player fails a mission.
    void OnMissionFailed(MissionId missionId);

    // Runs two simulated cycles to test detection without starting GTA III.
    bool RunLifecycleSelfTest();
}
