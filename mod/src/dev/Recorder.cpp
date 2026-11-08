#include <cmath>
#include <cstdio>
#include <string>
#include "plugin.h"
#include "CPad.h"
#include "CPlayerPed.h"
#include "CTimer.h"
#include "CVehicle.h"
#include "common.h"
#include "../Log.h"
#include "Actions.h"
#include "Recorder.h"

namespace {
    bool g_recording = false;
    // Game time when the recording started, and when the next position line is due.
    int g_startTime = 0;
    int g_nextPositionTime = 0;
    // The last actions and position written, so only changes reach the log.
    std::string g_lastActions;
    CVector g_lastPosition;
    float g_lastHeading = 0.0f;

    // Game time since the recording started, like "+12.3 s"; game time stops while the game is paused.
    std::string Elapsed() {
        char text[32];
        std::snprintf(text, sizeof(text), "+%.1f s", (CTimer::m_snTimeInMilliseconds - g_startTime) / 1000.0f);
        return text;
    }

    // Heading in degrees as SET_CAR_HEADING takes it: 0 faces north, 90 west.
    float HeadingDegrees(CPlaceable& placeable) {
        float degrees = placeable.GetHeading() * 180.0f / 3.14159265f;
        return degrees < 0.0f ? degrees + 360.0f : degrees;
    }

    // The game actions the player is doing now, in words: what the autopilot would have to do to repeat them.
    std::string CurrentActions() {
        CPad* pad = CPad::GetPad(0);
        std::string actions;
        auto add = [&actions](bool doing, const char* word) {
            if (!doing) return;
            if (!actions.empty()) actions += ", ";
            actions += word;
        };
        if (FindPlayerVehicle()) {
            add(pad->GetAccelerate() > 0, "accelerate");
            add(pad->GetBrake() > 0, "brake or reverse");
            const short steering = pad->GetSteeringLeftRight();
            add(steering < -32, "steer left");
            add(steering > 32, "steer right");
            add(pad->GetHandBrake() > 0, "handbrake");
            add(pad->GetHorn(), "horn");
            add(pad->GetExitVehicle(), "get out");
        } else {
            // In GTA III, pushing the stick up (or the forward key) gives a negative up-down value.
            const short forward = pad->GetPedWalkUpDown(), side = pad->GetPedWalkLeftRight();
            add(forward < -32, "walk forward");
            add(forward > 32, "walk back");
            add(side < -32, "walk left");
            add(side > 32, "walk right");
            add(pad->GetSprint(), "sprint");
            add(pad->GetExitVehicle(), "get in a car");
            add(pad->GetWeapon(), "attack");
        }
        return actions.empty() ? "nothing" : actions;
    }

    // One line with where the player, or their car, is and faces.
    void LogPosition() {
        CVehicle* car = FindPlayerVehicle();
        CPlaceable& placeable = car ? static_cast<CPlaceable&>(*car) : static_cast<CPlaceable&>(*FindPlayerPed());
        const CVector& position = placeable.GetPosition();
        g_lastPosition = position;
        g_lastHeading = HeadingDegrees(placeable);
        char text[160];
        std::snprintf(text, sizeof(text), "%s at %.2f, %.2f, %.2f, heading %.1f", car ? "in a car" : "on foot", position.x, position.y, position.z, g_lastHeading);
        Log::Write("Correction " + Elapsed() + ": " + text);
    }

    // True when the player, or their car, moved more than 2 m or turned more than 20 degrees since the last position line.
    bool MovedSinceLastLine() {
        CVehicle* car = FindPlayerVehicle();
        CPlaceable& placeable = car ? static_cast<CPlaceable&>(*car) : static_cast<CPlaceable&>(*FindPlayerPed());
        const float turned = std::fabs(HeadingDegrees(placeable) - g_lastHeading);
        return Actions::Distance2D(placeable.GetPosition(), g_lastPosition) > 2.0f || (turned > 20.0f && turned < 340.0f);
    }
}

void Recorder::Start(const std::string& why) {
    if (g_recording || !FindPlayerPed()) return;
    g_recording = true;
    g_startTime = CTimer::m_snTimeInMilliseconds;
    g_lastActions.clear();
    Log::Write("Correction recording starts: " + why);
    LogPosition();
    g_nextPositionTime = g_startTime + 1000;
}

void Recorder::Stop() {
    if (!g_recording) return;
    if (FindPlayerPed()) LogPosition();
    Log::Write("Correction recording stops");
    g_recording = false;
}

void Recorder::Update() {
    if (!g_recording || !FindPlayerPed()) return;
    const std::string actions = CurrentActions();
    if (actions != g_lastActions) {
        Log::Write("Correction " + Elapsed() + ": " + actions);
        g_lastActions = actions;
    }
    // A position line at most every second, only when the player moved or turned.
    if (CTimer::m_snTimeInMilliseconds < g_nextPositionTime) return;
    g_nextPositionTime = CTimer::m_snTimeInMilliseconds + 1000;
    if (MovedSinceLastLine()) LogPosition();
}
