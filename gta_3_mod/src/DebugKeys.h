#pragma once

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>

#include "Logger.h"
#include "PluginPaths.h"

// Central registry of the debug hotkeys, read from
// III.MissionSelector.keys.ini so they can be rebound/disabled without a
// rebuild. File format: one "action = key" per line. A key is a single
// letter/digit (P, 5), a function key (F1..F12) or SPACE/ENTER/TAB. A line
// commented with // or ; (or deleted entirely) leaves that action unbound, so
// it never fires. Code asks for a key with DebugKeys::Key("action").
namespace DebugKeys {
    struct DefaultBinding {
        const char* action;
        const char* key;
        const char* note;
    };

    inline constexpr DefaultBinding kDefaults[] = {
        {"test_print",         "P",   "PRINT_NOW brief test"},
        {"wanted_trap",        "M",   "3-star police trap"},
        {"clear_wanted",       "C",   "clear the police wanted level"},
        {"death",              "H",   "die + respawn at nearest hospital"},
        {"unlock_character",   "F2",  "unlock the next mission character"},
        {"validate_mission",   "F3",  "validate (V) the last-launched mission"},
        {"objective_teleport", "K",   "cycle-warp through this mission's recorded positions (F1 is the game replay key)"},
        {"record_position",    "J",   "save player position (² / 0xDE is an unreliable OEM key — use a letter)"},
        {"kill_npcs",          "L",   "kill all peds within 50 m of the player"},
        {"crossing_teleport",  "F5",  "cycle the inter-island choke points"},
        {"spawn_car",          "F6",  "spawn an Infernus"},
        {"give_weapons",       "G",   "give the player a full weapon loadout"},
        {"cycle_player_skin",  "O",   "toggle Claude normal PLAYERX / secondary PLAYER"},
        {"safehouse_teleport", "F9",  "cycle the safehouses"},
        {"vehicle_smoke",      "F11", "set current vehicle smoking"},
        {"vehicle_fire",       "F12", "set current vehicle on fire"},
    };

    inline std::map<std::string, int> g_bindings; // action -> Win32 VK (0 = unbound)

    inline std::string Trim(const std::string& v) {
        std::size_t a = 0;
        std::size_t b = v.size();
        while (a < b && std::isspace(static_cast<unsigned char>(v[a])) != 0) {
            ++a;
        }
        while (b > a && std::isspace(static_cast<unsigned char>(v[b - 1])) != 0) {
            --b;
        }
        return v.substr(a, b - a);
    }

    // Parse a key token into a Win32 virtual-key code; 0 if unrecognised.
    inline int ParseKey(const std::string& s) {
        if (s.empty()) {
            return 0;
        }
        // Raw Win32 virtual-key code in hex, e.g. 0xDE — needed for OEM keys
        // with no portable name (the FR "²" key left of "1", etc.).
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            return static_cast<int>(std::strtol(s.c_str(), nullptr, 16));
        }
        if ((s[0] == 'F' || s[0] == 'f') && s.size() >= 2) {
            const int n = std::atoi(s.c_str() + 1);
            if (n >= 1 && n <= 24) {
                return VK_F1 + (n - 1);
            }
        }
        if (s.size() == 1) {
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                return static_cast<int>(c);
            }
        }
        if (_stricmp(s.c_str(), "SPACE") == 0) {
            return VK_SPACE;
        }
        if (_stricmp(s.c_str(), "ENTER") == 0 || _stricmp(s.c_str(), "RETURN") == 0) {
            return VK_RETURN;
        }
        if (_stricmp(s.c_str(), "TAB") == 0) {
            return VK_TAB;
        }
        return 0;
    }

    inline void WriteDefaultFile(const std::string& path) {
        FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "w");
        if (!file) {
            Logger::Log("DebugKeys: failed to create '%s'", path.c_str());
            return;
        }
        std::fprintf(file, "; MissionSelector debug key bindings\n");
        std::fprintf(file, "; Format:  action = key   (one action per line)\n");
        std::fprintf(file, "; Keys:    a letter/digit (P, 5), a function key (F1..F12) or SPACE/ENTER/TAB\n");
        std::fprintf(file, "; Disable: comment a line with // or ; (or delete it)\n\n");
        for (const DefaultBinding& d : kDefaults) {
            std::fprintf(file, "%-20s = %-4s ; %s\n", d.action, d.key, d.note);
        }
        std::fclose(file);
    }

    // Strip a trailing inline comment introduced by ';' or '//'.
    inline std::string StripInlineComment(const std::string& value) {
        std::string out = value;
        const std::size_t semi = out.find(';');
        if (semi != std::string::npos) {
            out = out.substr(0, semi);
        }
        const std::size_t slashes = out.find("//");
        if (slashes != std::string::npos) {
            out = out.substr(0, slashes);
        }
        return Trim(out);
    }

    inline void Init() {
        g_bindings.clear();
        const std::string path = PluginPaths::InGameDir("III.MissionSelector.keys.ini");

        std::ifstream input(path);
        if (!input.is_open()) {
            for (const DefaultBinding& d : kDefaults) {
                g_bindings[d.action] = ParseKey(d.key);
            }
            WriteDefaultFile(path);
            Logger::Log("DebugKeys: no keys file, wrote defaults to '%s'", path.c_str());
            return;
        }

        std::string line;
        int count = 0;
        while (std::getline(input, line)) {
            const std::string t = Trim(line);
            if (t.empty() || t.rfind("//", 0) == 0 || t[0] == ';' || t[0] == '#') {
                continue; // blank or fully-commented line -> action stays unbound
            }
            const std::size_t eq = t.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string action = Trim(t.substr(0, eq));
            const std::string value = StripInlineComment(t.substr(eq + 1));
            if (action.empty()) {
                continue;
            }
            g_bindings[action] = ParseKey(value); // 0 if the token is unrecognised
            ++count;
        }
        Logger::Log("DebugKeys: loaded %d binding(s) from '%s'", count, path.c_str());
    }

    // Win32 VK for an action, or 0 when the action is unbound/disabled.
    // GetAsyncKeyState(0) reports "not pressed", so callers can poll it safely.
    inline int Key(const char* action) {
        const auto it = g_bindings.find(action);
        return it != g_bindings.end() ? it->second : 0;
    }
}
