#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include "plugin.h"
#include "CWorld.h"
#include "ClientLink.h"
#include "Items.h"
#include "Log.h"
#include "Markers.h"
#include "Message.h"
#include "Shops.h"

namespace {
    // How many items of our slot's list this game has applied: an item sent again after a reconnection is not applied twice.
    int g_applied = 0;
    // How many items of the list the slot's earlier sessions applied, from its save file (given_index): a game that starts skips the consumable ones among them.
    int g_givenIndex = 0;
    // The effects of the consumable items, as the client names them ("money"): what they gave is in the slot's save file, so a later game does not apply them again.
    std::set<std::string> g_consumables;

    // Does what the client says an item does: unlock a mission by its number, add money, or let the shops sell a weapon by its number; false for an effect this mod does not know.
    bool Apply(const std::string& effect, int value) {
        if (effect == "unlock") Markers::Unlock(static_cast<unsigned char>(value));
        else if (effect == "money") CWorld::Players[CWorld::PlayerInFocus].m_nMoney += value;
        else if (effect == "weapon") Shops::Unlock(value);
        else if (effect != "none") return false;
        return true;
    }

    // One "consumables <effect> ..." line: the effects whose items are applied once; the list takes the place of the one before, and is logged when it changes.
    void HandleConsumables(const std::string& line) {
        std::istringstream words(line);
        std::string command, effect, list;
        words >> command;
        std::set<std::string> consumables;
        while (words >> effect) consumables.insert(effect);
        if (consumables == g_consumables) return;
        g_consumables = consumables;
        for (const std::string& name : consumables) list += " " + name;
        Log::Write("Items: the consumable effects are" + (list.empty() ? std::string(" none") : list));
    }

    // One "given_index <count>" line: what the save file kept. A game that has applied no item yet takes it; one that has keeps its own count, which is newer.
    void HandleGivenIndex(const std::string& line) {
        std::istringstream words(line);
        std::string command;
        int count = -1;
        if (!(words >> command >> count) || count < 0) {
            Log::Write("Unknown message from the client: " + line);
            return;
        }
        if (g_applied > 0) return;
        g_givenIndex = count;
        Log::Write("Items: the save file says the earlier sessions applied " + std::to_string(count) + " items");
    }

    // One "item <index> <effect> <value> <name>" line: a new item is applied and shown; one this game already applied is skipped.
    void HandleItem(const std::string& line) {
        std::istringstream words(line);
        std::string command, effect, name;
        int index = -1, value = 0;
        const bool read = static_cast<bool>(words >> command >> index >> effect >> value);
        std::getline(words >> std::ws, name);
        if (!read || index < 0) {
            Log::Write("Unknown message from the client: " + line);
            return;
        }
        if (index < g_applied) return;
        g_applied = index + 1;
        // Applied in an earlier session: a lasting effect (a mission, a weapon) is applied again, silently, as this game starts without it; a consumable one is in the save file.
        if (index < g_givenIndex) {
            if (g_consumables.count(effect)) Log::Write("Items: item " + std::to_string(index) + " (" + name + ") was applied in an earlier session, the save file kept its " + effect);
            else Apply(effect, value);
            return;
        }
        if (!Apply(effect, value)) {
            Log::Write("Item " + std::to_string(index) + " (" + name + ") has an effect this mod does not know: " + effect);
            return;
        }
        Log::Write("Received item " + std::to_string(index) + ": " + name + " (" + effect + " " + std::to_string(value) + ")");
        Message::Show("Received: " + name);
    }

}

void Items::Install() {
    ClientLink::On("consumables", HandleConsumables);
    ClientLink::On("given_index", HandleGivenIndex);
    ClientLink::On("item", HandleItem);
}

int Items::Applied() {
    // Until this game has caught up with the earlier sessions, their count is the slot's: it never goes back.
    return std::max(g_applied, g_givenIndex);
}
