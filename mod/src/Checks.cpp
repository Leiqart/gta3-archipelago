#include <algorithm>
#include <string>
#include <vector>
#include "plugin.h"
#include "Checks.h"
#include "ClientLink.h"
#include "Log.h"
#include "Missions.h"

namespace {
    // A location of the apworld (its id in location_name_to_id) and the text key the game registers when its mission is passed.
    struct Location {
        const char* key;
        int id;
    };

    // The apworld's locations so far; Give Me Liberty covers Luigi's Girls, as the game plays both in one mission that registers LM1.
    constexpr Location kLocations[] = {
        {"LM1", 1},  // Give Me Liberty
        {"LM2", 3},  // Don't Spank Ma Bitch Up
        {"LM3", 4},  // Drive Misty For Me
    };

    // The locations finished since the game started, and whether the room was connected on the last frame.
    std::vector<int> g_done;
    bool g_roomConnected = false;

    // Sends one check to the client, which forwards it to the room.
    void Send(int id) {
        const bool sent = ClientLink::Send("check " + std::to_string(id));
        Log::Write("Check " + std::to_string(id) + (sent ? " sent to the client" : " not sent: the client is not running"));
    }

    // A mission passed: if the apworld has a location for it, that location is done and sent.
    void OnMissionPassed(const std::string&, const std::string& key) {
        for (const Location& location : kLocations) {
            if (key != location.key) continue;
            if (std::find(g_done.begin(), g_done.end(), location.id) == g_done.end()) g_done.push_back(location.id);
            Send(location.id);
            return;
        }
    }

    // Runs once per frame: the client drops the checks it gets without a room, so all of them are sent again when the room connects (it ignores those it has).
    void Process() {
        const bool connected = ClientLink::IsConnected();
        if (connected && !g_roomConnected) {
            for (int id : g_done) Send(id);
        }
        g_roomConnected = connected;
    }
}

void Checks::Install() {
    Missions::OnMissionPassed(OnMissionPassed);
    plugin::Events::gameProcessEvent += Process;
}
