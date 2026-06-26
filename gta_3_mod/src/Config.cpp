#include "Config.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>

#include "BuildConfig.h"
#include "Logger.h"
#include "PluginPaths.h"

namespace {
    constexpr char kConfigFilename[] = "III.MissionSelector.ini";
    constexpr char kMainScriptFilename[] = "data\\main.scm";
    constexpr char kArchipelagoScriptFilename[] = "data\\main_ap.scm";

    // Defaults live in BuildConfig.h so the shipped binary works with no ini.
    struct ConfigState {
        bool debugOpenIslands = AP_DEFAULT_DEBUG_OPEN_ISLANDS != 0;
        bool unlockMissionsAtomically = AP_DEFAULT_UNLOCK_MISSIONS_ATOMICALLY != 0;
        // When enabled, vanilla SCM contact markers may launch missions. The
        // hook still filters those launches through AP validation/order rules.
        bool useClassicMissionMarkers = AP_DEFAULT_USE_CLASSIC_MISSION_MARKERS != 0;
        // New AP worlds can replace contact-wide unlocks with one AP item per
        // mission (unlock_mission_luigi2, unlock_mission_joey1, ...).
        bool requireApMissionUnlockItems = AP_DEFAULT_REQUIRE_AP_MISSION_UNLOCK_ITEMS != 0;
        // The death hotkey (H) respawns Claude at the nearest hospital via a
        // real engine death. When true, his weapon inventory is snapshotted
        // before death and restored once the engine finishes the respawn,
        // overriding the engine's normal strip-on-death behaviour.
        bool keepWeaponsOnDeath = AP_DEFAULT_KEEP_WEAPONS_ON_DEATH != 0;
    };

    ConfigState g_config;

    std::string Trim(const std::string& value) {
        std::size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }

        std::size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }

        return value.substr(start, end - start);
    }

    std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    bool ParseBool(const std::string& value) {
        const std::string normalized = ToLower(Trim(value));
        return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
    }

    bool FileExists(const std::string& path) {
        const DWORD attributes = GetFileAttributesA(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool FilesIdentical(const std::string& leftPath, const std::string& rightPath) {
        std::ifstream left(leftPath, std::ios::binary | std::ios::ate);
        std::ifstream right(rightPath, std::ios::binary | std::ios::ate);
        if (!left.is_open() || !right.is_open()) {
            return false;
        }

        if (left.tellg() != right.tellg()) {
            return false;
        }

        left.seekg(0, std::ios::beg);
        right.seekg(0, std::ios::beg);

        std::array<char, 8192> leftBuffer{};
        std::array<char, 8192> rightBuffer{};

        while (left && right) {
            left.read(leftBuffer.data(), static_cast<std::streamsize>(leftBuffer.size()));
            right.read(rightBuffer.data(), static_cast<std::streamsize>(rightBuffer.size()));

            const std::streamsize leftBytes = left.gcount();
            const std::streamsize rightBytes = right.gcount();
            if (leftBytes != rightBytes) {
                return false;
            }
            if (leftBytes == 0) {
                break;
            }
            if (std::memcmp(leftBuffer.data(), rightBuffer.data(),
                            static_cast<std::size_t>(leftBytes)) != 0) {
                return false;
            }
        }

        return true;
    }

    bool CopyFileLogged(const std::string& source,
                        const std::string& target,
                        const char* operation) {
        if (CopyFileA(source.c_str(), target.c_str(), FALSE) == 0) {
            Logger::Log("Config: %s failed '%s' -> '%s' (error=%lu)",
                        operation,
                        source.c_str(),
                        target.c_str(),
                        GetLastError());
            return false;
        }

        Logger::Log("Config: %s '%s' -> '%s'",
                    operation,
                    source.c_str(),
                    target.c_str());
        return true;
    }

    void WriteDefaultConfig() {
        const std::string path = PluginPaths::InGameDir(kConfigFilename);

        FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "w");
        if (!file) {
            Logger::Log("Config: failed to create '%s'", path.c_str());
            return;
        }

        std::fprintf(file, "; MissionSelector startup config\n");
        std::fprintf(file, "; debug_open_islands=1 would select the old debug SCM path, but that variant is disabled\n");
        std::fprintf(file, "; unlock_missions_atomically=1 keeps later missions locked until the previous one is validated\n");
        std::fprintf(file, "; use_classic_mission_markers=1 lets vanilla mission markers launch AP-tracked missions\n");
        std::fprintf(file, "; require_ap_mission_unlock_items=1 requires AP unlock_mission_* items before mission launch\n");
        std::fprintf(file, "; keep_weapons_on_death=1 keeps your weapons when the H death hotkey respawns you at the hospital\n");
        std::fprintf(file, "debug_open_islands=%d\n", g_config.debugOpenIslands ? 1 : 0);
        std::fprintf(file, "unlock_missions_atomically=%d\n", g_config.unlockMissionsAtomically ? 1 : 0);
        std::fprintf(file, "use_classic_mission_markers=%d\n", g_config.useClassicMissionMarkers ? 1 : 0);
        std::fprintf(file, "require_ap_mission_unlock_items=%d\n", g_config.requireApMissionUnlockItems ? 1 : 0);
        std::fprintf(file, "keep_weapons_on_death=%d\n", g_config.keepWeaponsOnDeath ? 1 : 0);
        std::fclose(file);

        Logger::Log("Config: created default '%s' debug_open_islands=%d unlock_missions_atomically=%d use_classic_mission_markers=%d require_ap_mission_unlock_items=%d keep_weapons_on_death=%d",
                    path.c_str(),
                    g_config.debugOpenIslands ? 1 : 0,
                    g_config.unlockMissionsAtomically ? 1 : 0,
                    g_config.useClassicMissionMarkers ? 1 : 0,
                    g_config.requireApMissionUnlockItems ? 1 : 0,
                    g_config.keepWeaponsOnDeath ? 1 : 0);
    }

    void LoadFromDisk() {
        ConfigState nextConfig{};
        const std::string path = PluginPaths::InGameDir(kConfigFilename);

        std::ifstream input(path);
        if (!input.is_open()) {
            g_config = nextConfig;
            Logger::Log("Config: no config file, defaulting debug_open_islands=%d unlock_missions_atomically=%d use_classic_mission_markers=%d require_ap_mission_unlock_items=%d",
                        g_config.debugOpenIslands ? 1 : 0,
                        g_config.unlockMissionsAtomically ? 1 : 0,
                        g_config.useClassicMissionMarkers ? 1 : 0,
                        g_config.requireApMissionUnlockItems ? 1 : 0);
            WriteDefaultConfig();
            return;
        }

        std::string line;
        while (std::getline(input, line)) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            const std::string key = ToLower(Trim(line.substr(0, equals)));
            const std::string value = line.substr(equals + 1);

            if (key == "debug_open_islands") {
                nextConfig.debugOpenIslands = ParseBool(value);
            } else if (key == "unlock_missions_atomically") {
                nextConfig.unlockMissionsAtomically = ParseBool(value);
            } else if (key == "use_classic_mission_markers") {
                nextConfig.useClassicMissionMarkers = ParseBool(value);
            } else if (key == "require_ap_mission_unlock_items") {
                nextConfig.requireApMissionUnlockItems = ParseBool(value);
            } else if (key == "keep_weapons_on_death") {
                nextConfig.keepWeaponsOnDeath = ParseBool(value);
            }
        }

        g_config = nextConfig;
        Logger::Log("Config: loaded '%s' debug_open_islands=%d unlock_missions_atomically=%d use_classic_mission_markers=%d require_ap_mission_unlock_items=%d keep_weapons_on_death=%d",
                    path.c_str(),
                    g_config.debugOpenIslands ? 1 : 0,
                    g_config.unlockMissionsAtomically ? 1 : 0,
                    g_config.useClassicMissionMarkers ? 1 : 0,
                    g_config.requireApMissionUnlockItems ? 1 : 0,
                    g_config.keepWeaponsOnDeath ? 1 : 0);
    }
}

void Config::Init() {
    LoadFromDisk();
}

void Config::Refresh() {
    LoadFromDisk();
}

void Config::InstallConfiguredMainScript() {
    // The Archipelago boot always runs the AP mission script. When a separate
    // data/main_ap.scm is present, activate it as data/main.scm; otherwise the
    // shipped main.scm already IS the AP script, so keep it as-is.
    const std::string mainPath = PluginPaths::InGameDir(kMainScriptFilename);
    const std::string apPath = PluginPaths::InGameDir(kArchipelagoScriptFilename);

    if (!FileExists(apPath)) {
        Logger::Log("Config: no data/main_ap.scm, keeping current main.scm");
        return;
    }

    if (FilesIdentical(mainPath, apPath)) {
        Logger::Log("Config: archipelago main.scm already active");
        return;
    }

    if (CopyFileLogged(apPath, mainPath, "activated main script")) {
        Logger::Log("Config: active main.scm switched to archipelago variant");
    }
}

bool Config::DebugOpenIslands() {
    return g_config.debugOpenIslands;
}

bool Config::KeepWeaponsOnDeath() {
    return g_config.keepWeaponsOnDeath;
}

bool Config::UnlockMissionsAtomically() {
    return g_config.unlockMissionsAtomically;
}

bool Config::UseClassicMissionMarkers() {
    return g_config.useClassicMissionMarkers;
}

bool Config::RequireApMissionUnlockItems() {
    // Always on by design: AP unlock_mission_* items are THE progression key
    // in both launch modes (markers or pause-menu selector). The ini value is
    // kept for compatibility but no longer consulted — running a legacy seed
    // without mission items is not a supported configuration anymore.
    return true;
}
