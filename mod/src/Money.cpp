#include <windows.h>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include "plugin.h"
#include "CPlayerInfo.h"
#include "CWorld.h"
#include "ClientLink.h"
#include "Items.h"
#include "Log.h"
#include "Money.h"

namespace {
    // The least time between two reports, in milliseconds: money can change many times a second (a taxi fare, cars blown up), and the save file need not hear each change.
    constexpr DWORD kReportEvery = 1000;

    // Whether the game being played has taken the saved money: until then its own is not the slot's, and is not reported.
    bool g_taken = false;
    // The pair last sent to the client (items applied, money), and when; none while the room is not connected, so that it is sent again once it is.
    std::optional<std::pair<int, int>> g_reported;
    DWORD g_reportedAt = 0;

    CPlayerInfo& Player() { return CWorld::Players[CWorld::PlayerInFocus]; }

    // One "money <amount>" line: what the save file kept. A game that starts takes it, on its first frame and before its items, which come after it; a game already running keeps its own, which is newer.
    void HandleMoney(const std::string& line) {
        std::istringstream words(line);
        std::string command;
        int amount = -1;
        if (!(words >> command >> amount) || amount < 0) {
            Log::Write("Unknown message from the client: " + line);
            return;
        }
        if (g_taken) return;
        // The counter on the screen runs towards the money a few dollars a frame: it starts at the amount too.
        Player().m_nMoney = Player().m_nDisplayMoney = amount;
        g_taken = true;
        Log::Write("Money: the game starts with the $" + std::to_string(amount) + " the save file kept");
    }

    // Runs once per frame: tells the client the count of items applied and the player's money, read together, when either changes, and each time the room connects.
    void Process() {
        if (!g_taken || !ClientLink::IsConnected()) {
            g_reported.reset();
            return;
        }
        const std::pair<int, int> progress{Items::Applied(), Player().m_nMoney};
        if (g_reported == progress || (g_reported && GetTickCount() - g_reportedAt < kReportEvery)) return;
        if (!ClientLink::Send("progress " + std::to_string(progress.first) + " " + std::to_string(progress.second))) return;
        g_reported = progress;
        g_reportedAt = GetTickCount();
        Log::Write("Money: $" + std::to_string(progress.second) + " and " + std::to_string(progress.first) + " items applied, sent to the client");
    }
}

void Money::Install() {
    ClientLink::On("money", HandleMoney);
    // initScriptsEvent follows CTheScripts::Init, which every game that starts runs: its money is the game's own until the saved one comes.
    plugin::Events::initScriptsEvent += [] { g_taken = false; };
    plugin::Events::gameProcessEvent += Process;
}
