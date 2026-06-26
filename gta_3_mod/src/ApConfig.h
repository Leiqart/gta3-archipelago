#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

#include "PluginPaths.h"

namespace ApConfig {
    struct Settings {
        bool        autoConnect = true;
        std::string server      = "localhost:38281";
        std::string slot        = "GTA3Player";
        std::string password;
    };

    inline std::string Trim(const std::string& value) {
        std::size_t start = 0;
        while (start < value.size() &&
               std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }
        std::size_t end = value.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }
        return value.substr(start, end - start);
    }

    inline std::string Lower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    inline bool ParseBool(const std::string& value, bool def) {
        const std::string lowered = Lower(Trim(value));
        if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
            return true;
        }
        if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
            return false;
        }
        return def;
    }

    inline Settings LoadFromPath(const std::string& path) {
        Settings settings;
        std::ifstream input(path);
        if (!input.is_open()) {
            return settings;
        }

        std::string line;
        while (std::getline(input, line)) {
            const std::size_t comment = line.find(';');
            if (comment != std::string::npos) {
                line.erase(comment);
            }
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }
            const std::string key = Lower(Trim(line.substr(0, equals)));
            const std::string value = Trim(line.substr(equals + 1));
            if (key == "ap_autoconnect" || key == "autoconnect" || key == "auto_connect") {
                settings.autoConnect = ParseBool(value, settings.autoConnect);
            } else if (key == "ap_server" || key == "server" || key == "address") {
                if (!value.empty()) {
                    settings.server = value;
                }
            } else if (key == "ap_slot" || key == "slot" || key == "pseudo" || key == "name") {
                if (!value.empty()) {
                    settings.slot = value;
                }
            } else if (key == "ap_password" || key == "password") {
                settings.password = value;
            }
        }
        return settings;
    }

    inline Settings LoadFromGameDir() {
        return LoadFromPath(PluginPaths::InGameDir("AP_connexion.txt"));
    }
}
