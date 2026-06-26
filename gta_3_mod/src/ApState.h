#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Logger.h"
#include "PluginPaths.h"

// Structured Archipelago state (Bloc C of the master plan). Replaces the weak
// `checks=N` counter with a real, versioned state file the external bridge and
// the plugin both share:
//
//   III.Archipelago.state.json
//   {
//     "schema_version": 1,
//     "slot_name": "player1",
//     "seed_name": "local-test",
//     "run_active": true,
//     "start_mode": "archipelago",
//     "locations_checked": ["mission_8ball", ...],   // ids the game completed
//     "received_items":    ["unlock_luigi", ...]      // ids the server granted
//   }
//
// This first slice only loads/saves the file (round-trip) and exposes
// accessors; wiring it to the game logic (emit checks / apply items) comes in
// the next blocks. JSON is parsed with small, whitespace-tolerant, key-targeted
// helpers (the schema is fixed and fully under our control, so a full parser is
// unnecessary — and we avoid pulling in a heavy dependency).
namespace ApState {
    constexpr char kStateFilename[] = "III.Archipelago.state.json";
    // received_items lives in its own file, written by the external bridge and
    // re-read live by the plugin, so the two processes never write the same file.
    constexpr char kItemsFilename[] = "III.Archipelago.items.json";
    constexpr int  kSchemaVersion   = 1;

    inline int                      g_schemaVersion   = kSchemaVersion;
    inline std::string              g_slotName        = "player1";
    inline std::string              g_seedName        = "local-test";
    inline bool                     g_runActive       = false;
    inline std::string              g_startMode       = "archipelago";
    inline std::set<std::string>    g_locationsChecked;
    inline std::vector<std::string> g_receivedItems;
    inline int                      g_receivedItemsApplied = 0;
    // Session-only latches: AP progress is trusted only after the current bridge
    // session proved it is live. Checks/locations and received items are latched
    // separately so a fresh connect toast can validate server-side checks without
    // reviving stale local items before ReceivedItems arrives for this session.
    inline bool                     g_checkedStateTrustedThisSession = false;
    inline bool                     g_receivedItemsTrustedThisSession = false;

    inline std::string NormalizeReceivedItemName(const std::string& id) {
        if (id == "$2000" || id == "money") {
            return "cash";
        }
        return id;
    }

    // ---- tiny JSON readers (key-targeted, tolerant to whitespace/order) -----

    // Return the index just past `"key" :` (start of its value), or npos.
    inline std::size_t FindValue(const std::string& s, const std::string& key) {
        const std::string pat = "\"" + key + "\"";
        std::size_t p = s.find(pat);
        if (p == std::string::npos) {
            return std::string::npos;
        }
        p += pat.size();
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
        if (p >= s.size() || s[p] != ':') {
            return std::string::npos;
        }
        ++p;
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
        return p;
    }

    // Read a JSON string starting at s[pos]=='"'; advances pos past the close.
    inline bool ReadString(const std::string& s, std::size_t& pos, std::string& out) {
        if (pos >= s.size() || s[pos] != '"') {
            return false;
        }
        ++pos;
        out.clear();
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                ++pos;
                const char c = s[pos];
                out.push_back(c == 'n' ? '\n' : c == 't' ? '\t' : c);
            } else {
                out.push_back(s[pos]);
            }
            ++pos;
        }
        if (pos < s.size() && s[pos] == '"') {
            ++pos;
            return true;
        }
        return false;
    }

    inline std::string ReadScalarString(const std::string& s, const std::string& key,
                                        const std::string& def) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos) {
            return def;
        }
        std::string v;
        return ReadString(s, p, v) ? v : def;
    }

    inline bool ReadBool(const std::string& s, const std::string& key, bool def) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos) {
            return def;
        }
        if (s.compare(p, 4, "true") == 0)  return true;
        if (s.compare(p, 5, "false") == 0) return false;
        return def;
    }

    inline int ReadInt(const std::string& s, const std::string& key, int def) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos) {
            return def;
        }
        return std::atoi(s.c_str() + p);
    }

    template <typename Insert>
    inline void ReadStringArray(const std::string& s, const std::string& key, Insert insert) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos || p >= s.size() || s[p] != '[') {
            return;
        }
        ++p;
        while (p < s.size()) {
            while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
            if (p >= s.size() || s[p] == ']') break;
            if (s[p] == '"') {
                std::string v;
                if (ReadString(s, p, v)) insert(v);
            } else {
                ++p; // skip unexpected token
            }
            while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
            if (p < s.size() && s[p] == ',') ++p;
            else break;
        }
    }

    inline std::string Escape(const std::string& v) {
        std::string out;
        out.reserve(v.size());
        for (char c : v) {
            if (c == '"' || c == '\\') out.push_back('\\');
            out.push_back(c);
        }
        return out;
    }

    // ---- public API ---------------------------------------------------------

    inline void Save() {
        const std::string path = PluginPaths::InGameDir(kStateFilename);
        FILE* f = nullptr;
        fopen_s(&f, path.c_str(), "w");
        if (!f) {
            Logger::Log("ApState: failed to save '%s'", path.c_str());
            return;
        }
        std::fprintf(f, "{\n");
        std::fprintf(f, "  \"schema_version\": %d,\n", g_schemaVersion);
        std::fprintf(f, "  \"slot_name\": \"%s\",\n", Escape(g_slotName).c_str());
        std::fprintf(f, "  \"seed_name\": \"%s\",\n", Escape(g_seedName).c_str());
        std::fprintf(f, "  \"run_active\": %s,\n", g_runActive ? "true" : "false");
        std::fprintf(f, "  \"start_mode\": \"%s\",\n", Escape(g_startMode).c_str());
        std::fprintf(f, "  \"received_items_applied\": %d,\n", g_receivedItemsApplied);

        std::fprintf(f, "  \"locations_checked\": [");
        bool first = true;
        for (const std::string& id : g_locationsChecked) {
            std::fprintf(f, "%s\n    \"%s\"", first ? "" : ",", Escape(id).c_str());
            first = false;
        }
        std::fprintf(f, "%s]\n", first ? "" : "\n  ");

        std::fprintf(f, "}\n");
        std::fclose(f);
        Logger::Log("ApState: saved '%s' (locations=%zu)",
                    path.c_str(), g_locationsChecked.size());
    }

    // (Re)load received_items from the bridge-owned items file. Returns true if
    // the set changed, so callers can react (refresh menu / re-apply). Missing
    // file is fine (keeps whatever we already have).
    inline bool LoadReceivedItems() {
        std::ifstream in(PluginPaths::InGameDir(kItemsFilename), std::ios::binary);
        if (!in.is_open()) {
            return false;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string s = ss.str();
        std::vector<std::string> next;
        ReadStringArray(s, "received_items",
                        [&next](const std::string& v) {
                            next.push_back(NormalizeReceivedItemName(v));
                        });
        if (next == g_receivedItems) {
            return false;
        }
        g_receivedItems = std::move(next);
        return true;
    }

    inline void Init() {
        g_checkedStateTrustedThisSession = false;
        g_receivedItemsTrustedThisSession = false;
        const std::string path = PluginPaths::InGameDir(kStateFilename);
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            Logger::Log("ApState: no state file '%s', writing defaults", path.c_str());
            Save();
            return;
        }
        std::stringstream ss;
        ss << input.rdbuf();
        const std::string s = ss.str();

        g_schemaVersion = ReadInt(s, "schema_version", kSchemaVersion);
        g_slotName      = ReadScalarString(s, "slot_name", g_slotName);
        g_seedName      = ReadScalarString(s, "seed_name", g_seedName);
        g_runActive     = ReadBool(s, "run_active", g_runActive);
        g_startMode     = ReadScalarString(s, "start_mode", g_startMode);
        const bool hasReceivedItemsApplied =
            FindValue(s, "received_items_applied") != std::string::npos;
        g_receivedItemsApplied = ReadInt(s, "received_items_applied", g_receivedItemsApplied);
        g_locationsChecked.clear();
        ReadStringArray(s, "locations_checked",
                        [](const std::string& v) { g_locationsChecked.insert(v); });
        // Compat: older single-file setups kept received_items in the state
        // file; honour it, then let the dedicated items file override if present.
        g_receivedItems.clear();
        ReadStringArray(s, "received_items",
                        [](const std::string& v) {
                            g_receivedItems.push_back(NormalizeReceivedItemName(v));
                        });
        LoadReceivedItems();
        if (!hasReceivedItemsApplied) {
            // Migration path for older states: preserve prior "items only unlock
            // state" behaviour and do not retro-apply every historical item once.
            g_receivedItemsApplied = static_cast<int>(g_receivedItems.size());
        }
        if (g_receivedItemsApplied < 0) {
            g_receivedItemsApplied = 0;
        }
        if (g_receivedItemsApplied > static_cast<int>(g_receivedItems.size())) {
            g_receivedItemsApplied = static_cast<int>(g_receivedItems.size());
        }

        Logger::Log("ApState: loaded '%s' schema=%d slot='%s' seed='%s' run=%d "
                    "mode='%s' locations=%zu items=%zu applied=%d",
                    path.c_str(), g_schemaVersion, g_slotName.c_str(), g_seedName.c_str(),
                    g_runActive ? 1 : 0, g_startMode.c_str(),
                    g_locationsChecked.size(), g_receivedItems.size(),
                    g_receivedItemsApplied);
    }

    inline bool RefreshState() {
        const std::string path = PluginPaths::InGameDir(kStateFilename);
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            return false;
        }

        std::stringstream ss;
        ss << input.rdbuf();
        const std::string s = ss.str();

        const int nextSchemaVersion = ReadInt(s, "schema_version", g_schemaVersion);
        const std::string nextSlotName = ReadScalarString(s, "slot_name", g_slotName);
        const std::string nextSeedName = ReadScalarString(s, "seed_name", g_seedName);
        const bool nextRunActive = ReadBool(s, "run_active", g_runActive);
        const std::string nextStartMode = ReadScalarString(s, "start_mode", g_startMode);
        int nextReceivedItemsApplied = ReadInt(s, "received_items_applied", g_receivedItemsApplied);
        if (nextReceivedItemsApplied < 0) {
            nextReceivedItemsApplied = 0;
        }

        std::set<std::string> nextLocationsChecked;
        ReadStringArray(s, "locations_checked",
                        [&nextLocationsChecked](const std::string& v) {
                            nextLocationsChecked.insert(v);
                        });

        const bool changed =
            nextSchemaVersion != g_schemaVersion ||
            nextSlotName != g_slotName ||
            nextSeedName != g_seedName ||
            nextRunActive != g_runActive ||
            nextStartMode != g_startMode ||
            nextReceivedItemsApplied != g_receivedItemsApplied ||
            nextLocationsChecked != g_locationsChecked;

        if (!changed) {
            return false;
        }

        g_schemaVersion = nextSchemaVersion;
        g_slotName = nextSlotName;
        g_seedName = nextSeedName;
        g_runActive = nextRunActive;
        g_startMode = nextStartMode;
        g_receivedItemsApplied = nextReceivedItemsApplied;
        g_locationsChecked = std::move(nextLocationsChecked);

        Logger::Log("ApState: refreshed '%s' slot='%s' run=%d locations=%zu applied=%d",
                    path.c_str(), g_slotName.c_str(), g_runActive ? 1 : 0,
                    g_locationsChecked.size(), g_receivedItemsApplied);
        return true;
    }

    // Periodic live reload of bridge-granted items; returns true if it changed.
    inline bool RefreshItems() {
        return LoadReceivedItems();
    }

    inline void ResetForNewRun() {
        g_runActive = true;
        g_startMode = "archipelago";
        g_locationsChecked.clear();
        // Do not reset received_items/applied here. The AP inventory is owned by
        // the server and cumulative for the slot; replaying it on every local
        // fresh start duplicates cash/traps. New rooms are reset by the bridge
        // before connect, where items/applied are intentionally cleared.
        Logger::Log("ApState: reset checked locations for new run (items=%zu applied=%d)",
                    g_receivedItems.size(), g_receivedItemsApplied);
        Save();
    }

    inline bool TrustCheckedStateThisSession() {
        if (g_checkedStateTrustedThisSession) {
            return false;
        }
        g_checkedStateTrustedThisSession = true;
        Logger::Log("ApState: trusted live Archipelago checked-state for this session");
        return true;
    }

    inline bool TrustReceivedItemsThisSession() {
        if (g_receivedItemsTrustedThisSession) {
            return false;
        }
        g_receivedItemsTrustedThisSession = true;
        Logger::Log("ApState: trusted live Archipelago received-items for this session");
        return true;
    }

    inline bool IsCheckedStateTrustedThisSession() {
        return g_checkedStateTrustedThisSession;
    }

    inline bool IsReceivedItemsTrustedThisSession() {
        return g_receivedItemsTrustedThisSession;
    }

    inline bool IsLocationChecked(const std::string& id) {
        if (!g_checkedStateTrustedThisSession) {
            return false;
        }
        return g_locationsChecked.count(id) != 0;
    }

    // Record a freshly-completed location; persists immediately. Returns true if
    // it was newly added (so callers can fire a notification only once).
    inline bool AddLocationChecked(const std::string& id) {
        if (id.empty() || g_locationsChecked.count(id) != 0) {
            return false;
        }
        g_locationsChecked.insert(id);
        Logger::Log("ApState: location checked '%s' (total=%zu)",
                    id.c_str(), g_locationsChecked.size());
        Save();
        return true;
    }

    inline bool AddLocationChecked(const char* id) {
        return id && id[0] != 0 && AddLocationChecked(std::string(id));
    }

    inline bool HasItem(const std::string& id) {
        if (!g_receivedItemsTrustedThisSession) {
            return false;
        }
        for (const std::string& it : g_receivedItems) {
            if (it == id) return true;
        }
        return false;
    }

    inline const std::set<std::string>& LocationsChecked() {
        static const std::set<std::string> kEmpty;
        return g_checkedStateTrustedThisSession ? g_locationsChecked : kEmpty;
    }
    inline const std::vector<std::string>& ReceivedItems() {
        static const std::vector<std::string> kEmpty;
        return g_receivedItemsTrustedThisSession ? g_receivedItems : kEmpty;
    }
    inline int ReceivedItemsAppliedCount() {
        return g_receivedItemsTrustedThisSession ? g_receivedItemsApplied : 0;
    }
    inline const std::string&              SlotName()         { return g_slotName; }
    inline const std::string&              SeedName()         { return g_seedName; }
    inline const std::string&              StartMode()        { return g_startMode; }
    inline bool                            RunActive()        { return g_runActive; }

    inline void SetReceivedItemsAppliedCount(int count) {
        const int clamped = count < 0 ? 0
            : (count > static_cast<int>(g_receivedItems.size())
                ? static_cast<int>(g_receivedItems.size())
                : count);
        if (g_receivedItemsApplied == clamped) {
            return;
        }
        g_receivedItemsApplied = clamped;
        Save();
    }
}
