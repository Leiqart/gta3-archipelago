#include <algorithm>
#include <string>
#include <vector>
#include "plugin.h"
#include "Checks.h"
#include "ClientLink.h"
#include "Log.h"
#include "Missions.h"

namespace {
    // The text keys of the missions passed since the game started, and whether the room was connected on the last frame.
    std::vector<std::string> g_passed;
    bool g_roomConnected = false;

    // Sends one check to the client, which looks up its location and forwards it to the room.
    void Send(const std::string& key) {
        const bool sent = ClientLink::Send("check " + key);
        Log::Write("Check " + key + (sent ? " sent to the client" : " not sent: the client is not running"));
    }

    // A mission passed: its key is sent, whether or not it is a location; the client knows which ones are.
    void OnMissionPassed(const std::string&, const std::string& key) {
        if (std::find(g_passed.begin(), g_passed.end(), key) == g_passed.end()) g_passed.push_back(key);
        Send(key);
    }

    // Runs once per frame: the client drops the checks it gets without a room, so all of them are sent again when the room connects (it ignores those it has).
    void Process() {
        const bool connected = ClientLink::IsConnected();
        if (connected && !g_roomConnected) {
            for (const std::string& key : g_passed) Send(key);
        }
        g_roomConnected = connected;
    }
}

void Checks::Install() {
    Missions::OnMissionPassed(OnMissionPassed);
    plugin::Events::gameProcessEvent += Process;
}
