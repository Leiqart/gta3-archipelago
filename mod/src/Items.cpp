#include <iterator>
#include <sstream>
#include <string>
#include "plugin.h"
#include "CWorld.h"
#include "ClientLink.h"
#include "Items.h"
#include "Log.h"
#include "Markers.h"
#include "Message.h"

namespace {
    // The apworld's item ids (item_name_to_id); never renumbered.
    constexpr int kProgressiveLuigiMission = 1;
    constexpr int kMoney = 2;
    constexpr int kDontSpankUnlock = 4;
    constexpr int kDriveMistyUnlock = 5;

    // Luigi's missions in the order Progressive Luigi Mission unlocks them: Don't Spank Ma Bitch Up, then Drive Misty For Me.
    constexpr unsigned char kLuigiChain[] = {20, 21};

    // How many items of our slot's list this game has applied, and how many Progressive Luigi Missions among them.
    int g_applied = 0;
    std::size_t g_luigiReceived = 0;

    // Applies one item in the game; false for an id this mod does not know.
    bool Apply(int id) {
        switch (id) {
        case kProgressiveLuigiMission:
            if (g_luigiReceived < std::size(kLuigiChain)) Markers::Unlock(kLuigiChain[g_luigiReceived]);
            ++g_luigiReceived;
            return true;
        case kMoney:
            CWorld::Players[CWorld::PlayerInFocus].m_nMoney += 10000;
            return true;
        case kDontSpankUnlock:
            Markers::Unlock(20);
            return true;
        case kDriveMistyUnlock:
            Markers::Unlock(21);
            return true;
        default:
            return false;
        }
    }

    // One "item <index> <id> <name>" line: a new item is applied and shown; one this game already applied, sent again after a reconnection, is skipped.
    void HandleLine(const std::string& line) {
        std::istringstream words(line);
        std::string command, name;
        int index = -1, id = -1;
        words >> command >> index >> id;
        std::getline(words >> std::ws, name);
        if (command != "item" || index < 0 || id < 0) {
            Log::Write("Unknown message from the client: " + line);
            return;
        }
        if (index < g_applied) return;
        g_applied = index + 1;
        if (!Apply(id)) {
            Log::Write("Item " + std::to_string(id) + " (" + name + ") is unknown to this mod");
            return;
        }
        Log::Write("Received item " + std::to_string(index) + ": " + name);
        Message::Show("Received: " + name);
    }

    // Runs once per frame: applies what the client sent since the last frame.
    void Process() {
        for (const std::string& line : ClientLink::TakeLines()) HandleLine(line);
    }
}

void Items::Install() {
    plugin::Events::gameProcessEvent += Process;
}
