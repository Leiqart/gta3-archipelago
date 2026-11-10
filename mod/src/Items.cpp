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
    // How many items of our slot's list this game has applied: an item sent again after a reconnection is not applied twice.
    int g_applied = 0;

    // Does what the client says an item does: unlock a mission by its number, or add money; false for an effect this mod does not know.
    bool Apply(const std::string& effect, int value) {
        if (effect == "unlock") Markers::Unlock(static_cast<unsigned char>(value));
        else if (effect == "money") CWorld::Players[CWorld::PlayerInFocus].m_nMoney += value;
        else if (effect != "none") return false;
        return true;
    }

    // One "item <index> <effect> <value> <name>" line: a new item is applied and shown; one this game already applied is skipped.
    void HandleLine(const std::string& line) {
        std::istringstream words(line);
        std::string command, effect, name;
        int index = -1, value = 0;
        words >> command >> index >> effect >> value;
        std::getline(words >> std::ws, name);
        if (command != "item" || index < 0) {
            Log::Write("Unknown message from the client: " + line);
            return;
        }
        if (index < g_applied) return;
        g_applied = index + 1;
        if (!Apply(effect, value)) {
            Log::Write("Item " + std::to_string(index) + " (" + name + ") has an effect this mod does not know: " + effect);
            return;
        }
        Log::Write("Received item " + std::to_string(index) + ": " + name + " (" + effect + " " + std::to_string(value) + ")");
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
