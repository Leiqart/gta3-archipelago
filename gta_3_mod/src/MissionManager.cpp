#include "MissionManager.h"

#include "Log.h"
#include "MissionTable.h"

namespace {
    using MissionManager::kUnknownMission;

    MissionManager::EventSink g_eventSink = nullptr;
    MissionManager::MissionId g_pendingMission = kUnknownMission;
    MissionManager::MissionId g_currentMission = kUnknownMission;
    int g_missionsPassedAtEnter = 0;
    int g_inactiveFrames = 0;
    bool g_missionWasActive = false;
    bool g_scriptedSuccess = false;

    // Clears the current cycle without forgetting a mission that was just announced.
    void ClearCurrentMission() {
        g_currentMission = kUnknownMission;
        g_missionsPassedAtEnter = 0;
        g_inactiveFrames = 0;
        g_missionWasActive = false;
        g_scriptedSuccess = false;
    }

    // Forwards a mission event to the plugged-in receiver, if any.
    void Emit(MissionManager::Event event, MissionManager::MissionId missionId) {
        if (g_eventSink) {
            g_eventSink(event, missionId);
        }
    }

    // Gives the Archipelago location of a mission to keep the log readable.
    const char* DescribeLocation(MissionManager::MissionId missionId) {
        const char* location = MissionTable::LocationFor(missionId);
        return location ? location : "sans lieu AP";
    }
}

// Replaces the event receiver, for example with the fake game used by tests.
void MissionManager::SetEventSink(EventSink sink) {
    g_eventSink = sink;
}

// Puts the detector back in its initial state.
void MissionManager::Reset() {
    g_pendingMission = kUnknownMission;
    ClearCurrentMission();
}

// Keeps the mission index until its script becomes active.
void MissionManager::NotifyMissionLaunch(MissionId missionId) {
    g_pendingMission = missionId;
}

bool MissionManager::ConfirmScriptedSuccess(MissionId missionId) {
    if (!g_missionWasActive || g_currentMission != missionId) return false;
    g_scriptedSuccess = true;
    return true;
}

// Turns successive memory snapshots into one-shot mission events.
MissionManager::Event MissionManager::Update(bool missionScriptRunning,
                                             int missionsPassed) {
    if (missionScriptRunning) {
        // A new launch is a boundary even if the old script's flag never stayed
        // down for five frames (INTRO -> EIGHT is the vanilla example).
        if (g_missionWasActive && g_pendingMission != kUnknownMission) {
            const auto previous = g_currentMission;
            const bool succeeded = g_scriptedSuccess || missionsPassed > g_missionsPassedAtEnter;
            ClearCurrentMission();
            if (succeeded) OnMissionSuccess(previous);
            else OnMissionFailed(previous);
        }
        g_inactiveFrames = 0;

        if (!g_missionWasActive) {
            g_missionWasActive = true;
            g_currentMission = g_pendingMission;
            g_pendingMission = kUnknownMission;
            g_missionsPassedAtEnter = missionsPassed;
            OnMissionEnter(g_currentMission);
            return Event::Enter;
        }

        OnMissionStay(g_currentMission);
        return Event::Stay;
    }

    if (!g_missionWasActive) {
        return Event::None;
    }

    // A mission only counts as over after several inactive frames.
    ++g_inactiveFrames;
    if (g_inactiveFrames < kEndConfirmationFrames) {
        return Event::None;
    }

    const MissionId missionId = g_currentMission;
    const bool succeeded = g_scriptedSuccess || missionsPassed > g_missionsPassedAtEnter;
    ClearCurrentMission();

    if (succeeded) {
        OnMissionSuccess(missionId);
        return Event::Success;
    }

    OnMissionFailed(missionId);
    return Event::Failed;
}

// Reports entering a mission; in-game detection comes later.
void MissionManager::OnMissionEnter(MissionId missionId) {
    Log::Write("Mission %d (%s): entree", missionId, DescribeLocation(missionId));
    Emit(Event::Enter, missionId);
}

// Reports a mission success; sending it to Archipelago comes later.
void MissionManager::OnMissionSuccess(MissionId missionId) {
    Log::Write("Mission %d (%s): reussie", missionId, DescribeLocation(missionId));
    Emit(Event::Success, missionId);
}

// Placeholder for the work done while a mission is active.
void MissionManager::OnMissionStay(MissionId missionId) {
    // No log here: this may be called on every game frame.
    (void)missionId;
}

// Reports a mission failure; the mod's reaction comes later.
void MissionManager::OnMissionFailed(MissionId missionId) {
    Log::Write("Mission %d (%s): echouee", missionId, DescribeLocation(missionId));
    Emit(Event::Failed, missionId);
}

// Checks the enter, stay, success and failure transitions with fake data.
bool MissionManager::RunLifecycleSelfTest() {
    Reset();
    NotifyMissionLaunch(7);

    bool valid = Update(false, 12) == Event::None;
    valid = valid && Update(true, 12) == Event::Enter;
    valid = valid && Update(true, 12) == Event::Stay;
    for (int frame = 1; frame < kEndConfirmationFrames; ++frame) {
        valid = valid && Update(false, 13) == Event::None;
    }
    valid = valid && Update(false, 13) == Event::Success;
    valid = valid && Update(false, 13) == Event::None;

    Reset();
    NotifyMissionLaunch(11);
    valid = valid && Update(true, 21) == Event::Enter;
    for (int frame = 1; frame < kEndConfirmationFrames; ++frame) {
        valid = valid && Update(false, 21) == Event::None;
    }
    valid = valid && Update(false, 21) == Event::Failed;
    valid = valid && Update(false, 21) == Event::None;

    Reset();
    return valid;
}
