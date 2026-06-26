#include "RunState.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

#include "Logger.h"
#include "PluginPaths.h"

namespace {
    constexpr char kStateFilename[] = "III.MissionSelector.state.ini";

    struct State {
        bool runActive = false;
        int checks     = 0;
        int mapState   = 0;
        // Number of unlocked mission characters (contacts), in MissionBucket
        // order starting at Luigi. 1 = only Luigi (run start); F2 raises it.
        int unlockedCharacters = 1;
        // Player money, mirrored from the live in-game cash so it persists across
        // disconnect/reconnect. Independent of mission validation.
        int money = 0;
        // Live-ped mirror: weapon slots (type:ammo), vitals and last position,
        // restored once per session like money. health <= 0 = nothing saved.
        int weaponType[RunState::kPersistWeaponSlots] = {};
        int weaponAmmo[RunState::kPersistWeaponSlots] = {};
        int health = 0;
        int armor  = 0;
        bool hasPosition = false;
        float posX = 0.0f;
        float posY = 0.0f;
        float posZ = 0.0f;
        // Hidden-package progression (engine count resets every new game).
        int packages = 0;
        std::set<int> packageIndices;
        // One-shot: boot the next run start through the vanilla-style intro.
        bool bootIntroRequested = false;
        std::set<int> unlockedBuckets;
        // Actual main.scm indices of validated (completed) missions.
        std::set<int> validated;
        // Synthetic selector-only completions that do not map 1:1 to a compiled
        // mission index (currently the split "Give Me Liberty" launch).
        std::set<std::string> validatedExtra;
    };

    State g_state;
    bool  g_runStartPending = false;
    // Session-only: true once the bridge confirms a live server connection.
    // Never read from / written to disk — every boot starts disconnected.
    bool  g_serverConnected = false;
    bool  g_skipIntroOnNextNewGame = false;
    int   g_lastLaunchedMission = -1;
    int   g_pendingValidationMission = -1;
    std::string g_pendingValidationKey;

    std::string JoinValidated(const std::set<int>& set) {
        std::string out;
        for (const int index : set) {
            if (!out.empty()) {
                out.push_back(',');
            }
            out += std::to_string(index);
        }
        return out;
    }

    std::string JoinValidatedExtra(const std::set<std::string>& set) {
        std::string out;
        for (const std::string& key : set) {
            if (!out.empty()) {
                out.push_back(',');
            }
            out += key;
        }
        return out;
    }

    std::string JoinUnlockedBuckets(const std::set<int>& set) {
        std::string out;
        for (const int bucket : set) {
            if (!out.empty()) {
                out.push_back(',');
            }
            out += std::to_string(bucket);
        }
        return out;
    }

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

    void ParseValidated(const std::string& value, std::set<int>& out) {
        std::size_t pos = 0;
        while (pos < value.size()) {
            std::size_t comma = value.find(',', pos);
            if (comma == std::string::npos) {
                comma = value.size();
            }
            const std::string token = Trim(value.substr(pos, comma - pos));
            if (!token.empty()) {
                const int index = std::atoi(token.c_str());
                if (index >= 0) {
                    out.insert(index);
                }
            }
            pos = comma + 1;
        }
    }

    void ParseValidatedExtra(const std::string& value, std::set<std::string>& out) {
        std::size_t pos = 0;
        while (pos < value.size()) {
            std::size_t comma = value.find(',', pos);
            if (comma == std::string::npos) {
                comma = value.size();
            }
            const std::string token = Trim(value.substr(pos, comma - pos));
            if (!token.empty()) {
                out.insert(token);
            }
            pos = comma + 1;
        }
    }

    void ParseUnlockedBuckets(const std::string& value, std::set<int>& out) {
        std::size_t pos = 0;
        while (pos < value.size()) {
            std::size_t comma = value.find(',', pos);
            if (comma == std::string::npos) {
                comma = value.size();
            }
            const std::string token = Trim(value.substr(pos, comma - pos));
            if (!token.empty()) {
                const int bucket = std::atoi(token.c_str());
                if (bucket > 0) {
                    out.insert(bucket);
                }
            }
            pos = comma + 1;
        }
    }

    // weapons= persists as "type:ammo,type:ammo,..." in slot order; empty
    // entries (type 0) are written as "0:0" to keep slot positions stable.
    std::string JoinWeapons(const State& state) {
        std::string out;
        for (int slot = 0; slot < RunState::kPersistWeaponSlots; ++slot) {
            if (!out.empty()) {
                out.push_back(',');
            }
            out += std::to_string(state.weaponType[slot]);
            out.push_back(':');
            out += std::to_string(state.weaponAmmo[slot]);
        }
        return out;
    }

    void ParseWeapons(const std::string& value, State& state) {
        std::size_t pos = 0;
        int slot = 0;
        while (pos < value.size() && slot < RunState::kPersistWeaponSlots) {
            std::size_t comma = value.find(',', pos);
            if (comma == std::string::npos) {
                comma = value.size();
            }
            const std::string token = Trim(value.substr(pos, comma - pos));
            const std::size_t colon = token.find(':');
            if (colon != std::string::npos) {
                state.weaponType[slot] = std::atoi(token.substr(0, colon).c_str());
                state.weaponAmmo[slot] = std::atoi(token.c_str() + colon + 1);
            }
            ++slot;
            pos = comma + 1;
        }
    }

    void ParsePosition(const std::string& value, State& state) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (std::sscanf(value.c_str(), "%f,%f,%f", &x, &y, &z) == 3) {
            state.posX = x;
            state.posY = y;
            state.posZ = z;
            state.hasPosition = true;
        }
    }

    void LoadFromDisk() {
        State nextState{};
        const std::string path = PluginPaths::InGameDir(kStateFilename);

        std::ifstream input(path);
        if (!input.is_open()) {
            g_state = nextState;
            Logger::Log("RunState: no state file '%s', defaulting to run=0 checks=0",
                        path.c_str());
            return;
        }

        std::string line;
        while (std::getline(input, line)) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            const std::string key = Trim(line.substr(0, equals));
            const std::string value = Trim(line.substr(equals + 1));

            if (key == "run_active") {
                nextState.runActive = value == "1" || value == "true" || value == "TRUE";
            } else if (key == "checks") {
                nextState.checks = std::max(0, std::atoi(value.c_str()));
            } else if (key == "map_state") {
                nextState.mapState = std::clamp(std::atoi(value.c_str()), 0, 2);
            } else if (key == "unlocked_chars") {
                nextState.unlockedCharacters = std::max(1, std::atoi(value.c_str()));
            } else if (key == "money") {
                nextState.money = std::max(0, std::atoi(value.c_str()));
            } else if (key == "weapons") {
                ParseWeapons(value, nextState);
            } else if (key == "health") {
                nextState.health = std::atoi(value.c_str());
            } else if (key == "armor") {
                nextState.armor = std::max(0, std::atoi(value.c_str()));
            } else if (key == "pos") {
                ParsePosition(value, nextState);
            } else if (key == "packages") {
                nextState.packages = std::max(0, std::atoi(value.c_str()));
            } else if (key == "package_idx") {
                ParseValidated(value, nextState.packageIndices);
            } else if (key == "boot_intro_requested") {
                nextState.bootIntroRequested = value == "1";
            } else if (key == "unlocked_buckets") {
                ParseUnlockedBuckets(value, nextState.unlockedBuckets);
            } else if (key == "validated" || key == "validated_extra") {
                // Mission validation is Archipelago server state, not local
                // resume state. Old builds persisted these fields and could
                // resurrect Give Me Liberty / Luigi progress before the live
                // server confirmed the corresponding checked locations.
                continue;
            }
        }

        g_state = nextState;
        Logger::Log("RunState: loaded '%s' run=%d checks=%d map_state=%d unlocked_chars=%d unlocked_buckets=%zu validated=%zu extra=%zu",
                    path.c_str(),
                    g_state.runActive ? 1 : 0,
                    g_state.checks,
                    g_state.mapState,
                    g_state.unlockedCharacters,
                    g_state.unlockedBuckets.size(),
                    g_state.validated.size(),
                    g_state.validatedExtra.size());
    }

    void SaveToDisk() {
        const std::string path = PluginPaths::InGameDir(kStateFilename);
        FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "w");
        if (!file) {
            Logger::Log("RunState: failed to save '%s'", path.c_str());
            return;
        }

        std::fprintf(file, "run_active=%d\n", g_state.runActive ? 1 : 0);
        std::fprintf(file, "checks=%d\n", g_state.checks);
        std::fprintf(file, "map_state=%d\n", g_state.mapState);
        std::fprintf(file, "unlocked_chars=%d\n", g_state.unlockedCharacters);
        std::fprintf(file, "money=%d\n", g_state.money);
        std::fprintf(file, "weapons=%s\n", JoinWeapons(g_state).c_str());
        std::fprintf(file, "health=%d\n", g_state.health);
        std::fprintf(file, "armor=%d\n", g_state.armor);
        if (g_state.hasPosition) {
            std::fprintf(file, "pos=%.2f,%.2f,%.2f\n",
                         g_state.posX, g_state.posY, g_state.posZ);
        }
        std::fprintf(file, "packages=%d\n", g_state.packages);
        std::fprintf(file, "package_idx=%s\n",
                     JoinValidated(g_state.packageIndices).c_str());
        std::fprintf(file, "boot_intro_requested=%d\n",
                     g_state.bootIntroRequested ? 1 : 0);
        std::fprintf(file, "unlocked_buckets=%s\n",
                     JoinUnlockedBuckets(g_state.unlockedBuckets).c_str());
        // Do not persist mission validation locally. The in-memory sets still
        // debounce the current session; durable progression comes from
        // Archipelago checked locations only.
        std::fprintf(file, "validated=\n");
        std::fprintf(file, "validated_extra=\n");
        std::fclose(file);

        Logger::Log("RunState: saved '%s' run=%d checks=%d map_state=%d unlocked_chars=%d unlocked_buckets=%zu validated=%zu extra=%zu",
                    path.c_str(),
                    g_state.runActive ? 1 : 0,
                    g_state.checks,
                    g_state.mapState,
                    g_state.unlockedCharacters,
                    g_state.unlockedBuckets.size(),
                    g_state.validated.size(),
                    g_state.validatedExtra.size());
    }
}

void RunState::Init() {
    g_runStartPending = false;
    g_skipIntroOnNextNewGame = false;
    g_lastLaunchedMission = -1;
    g_pendingValidationMission = -1;
    g_pendingValidationKey.clear();
    LoadFromDisk();
}

void RunState::Refresh() {
    LoadFromDisk();
}

bool RunState::IsRunActive() {
    return g_state.runActive;
}

void RunState::SetServerConnected(bool connected) {
    g_serverConnected = connected;
}

bool RunState::ServerConnected() {
    return g_serverConnected;
}

bool RunState::IsRunLive() {
    return g_state.runActive && g_serverConnected;
}

bool RunState::IsRunStartPending() {
    return g_runStartPending;
}

bool RunState::ShouldSkipIntroOnNextNewGame() {
    return g_skipIntroOnNextNewGame;
}

int RunState::Checks() {
    return g_state.checks;
}

int RunState::MapState() {
    return g_state.mapState;
}

void RunState::SetMapState(int state) {
    g_state.mapState = std::clamp(state, 0, 2);
    SaveToDisk();
}

int RunState::UnlockedMissionCount(int missionCount) {
    if (!g_state.runActive || missionCount <= 0) {
        return 0;
    }
    return std::clamp(g_state.checks, 0, missionCount);
}

bool RunState::IsMissionUnlocked(int missionIndex, int missionCount) {
    return missionIndex >= 0 && missionIndex < UnlockedMissionCount(missionCount);
}

void RunState::ActivateRun() {
    g_state.runActive = true;
    g_runStartPending = false;
    SaveToDisk();
}

void RunState::BeginNewRun() {
    g_state.runActive = true;
    g_state.checks = 0;
    // A fresh run starts on the first island only; the world reloads with the
    // inter-island bridge barriers in place, so map progression resets to 0.
    g_state.mapState = 0;
    // Only Luigi (the first character) is unlocked at the start of a run.
    g_state.unlockedCharacters = 1;
    // A fresh run starts broke, unarmed, at full default vitals, with no
    // hidden-package progress and no saved position.
    g_state.money = 0;
    for (int slot = 0; slot < RunState::kPersistWeaponSlots; ++slot) {
        g_state.weaponType[slot] = 0;
        g_state.weaponAmmo[slot] = 0;
    }
    g_state.health = 0;
    g_state.armor = 0;
    g_state.hasPosition = false;
    g_state.posX = g_state.posY = g_state.posZ = 0.0f;
    g_state.packages = 0;
    g_state.packageIndices.clear();
    g_state.bootIntroRequested = false;
    g_state.unlockedBuckets.clear();
    // No mission validated yet on a fresh run.
    g_state.validated.clear();
    g_state.validatedExtra.clear();
    g_lastLaunchedMission = -1;
    g_pendingValidationMission = -1;
    g_pendingValidationKey.clear();
    g_runStartPending = false;
    SaveToDisk();
}

void RunState::EndRun() {
    g_state.runActive = false;
    g_runStartPending = false;
    g_skipIntroOnNextNewGame = false;
    g_lastLaunchedMission = -1;
    g_pendingValidationMission = -1;
    g_pendingValidationKey.clear();
    SaveToDisk();
}

void RunState::SetChecks(int checks) {
    g_state.checks = std::max(0, checks);
    SaveToDisk();
}

int RunState::UnlockedCharacters() {
    return g_state.unlockedCharacters;
}

void RunState::SetUnlockedCharacters(int count) {
    g_state.unlockedCharacters = std::max(1, count);
    SaveToDisk();
}

bool RunState::IsBucketUnlocked(int bucketOrdinal) {
    return bucketOrdinal > 0 && g_state.unlockedBuckets.count(bucketOrdinal) != 0;
}

void RunState::UnlockBucket(int bucketOrdinal) {
    if (bucketOrdinal <= 0) {
        return;
    }
    const auto inserted = g_state.unlockedBuckets.insert(bucketOrdinal);
    if (inserted.second) {
        Logger::Log("RunState: unlocked bucket=%d (explicit total=%zu)",
                    bucketOrdinal, g_state.unlockedBuckets.size());
        SaveToDisk();
    }
}

bool RunState::IsMissionValidated(int actualMissionIndex) {
    return g_state.validated.count(actualMissionIndex) != 0;
}

void RunState::ValidateMission(int actualMissionIndex) {
    if (actualMissionIndex < 0) {
        return;
    }
    const auto inserted = g_state.validated.insert(actualMissionIndex);
    if (inserted.second) {
        Logger::Log("RunState: validated mission index=%d (total=%zu)",
                    actualMissionIndex, g_state.validated.size());
        SaveToDisk();
    }
}

bool RunState::IsSyntheticMissionValidated(const char* key) {
    return key && key[0] != 0 && g_state.validatedExtra.count(key) != 0;
}

void RunState::ValidateSyntheticMission(const char* key) {
    if (!key || key[0] == 0) {
        return;
    }
    const auto inserted = g_state.validatedExtra.insert(key);
    if (inserted.second) {
        Logger::Log("RunState: validated synthetic mission key='%s' (total=%zu)",
                    key, g_state.validatedExtra.size());
        SaveToDisk();
    }
}

int RunState::ValidatedMissionCount() {
    return static_cast<int>(g_state.validated.size());
}

int RunState::Money() {
    return g_state.money;
}

void RunState::SetMoney(int amount) {
    const int clamped = std::max(0, amount);
    if (g_state.money == clamped) {
        return;
    }
    g_state.money = clamped;
    SaveToDisk();
}

bool RunState::HasAnyRunProgress() {
    if (g_state.money > 0 ||
        g_state.packages > 0 ||
        g_state.hasPosition) {
        return true;
    }
    for (int slot = 0; slot < kPersistWeaponSlots; ++slot) {
        if (g_state.weaponType[slot] != 0) {
            return true;
        }
    }
    return false;
}

bool RunState::IsBootIntroRequested() {
    return g_state.bootIntroRequested;
}

void RunState::RequestBootIntro() {
    if (g_state.bootIntroRequested) {
        return;
    }
    g_state.bootIntroRequested = true;
    SaveToDisk();
}

void RunState::ClearBootIntroRequest() {
    if (!g_state.bootIntroRequested) {
        return;
    }
    g_state.bootIntroRequested = false;
    SaveToDisk();
}

bool RunState::SavedWeapons(int* types, int* ammo) {
    bool any = false;
    for (int slot = 0; slot < kPersistWeaponSlots; ++slot) {
        types[slot] = g_state.weaponType[slot];
        ammo[slot]  = g_state.weaponAmmo[slot];
        if (types[slot] != 0) {
            any = true;
        }
    }
    return any;
}

void RunState::SetSavedWeapons(const int* types, const int* ammo) {
    bool changed = false;
    for (int slot = 0; slot < kPersistWeaponSlots; ++slot) {
        if (g_state.weaponType[slot] != types[slot] ||
            g_state.weaponAmmo[slot] != ammo[slot]) {
            g_state.weaponType[slot] = types[slot];
            g_state.weaponAmmo[slot] = ammo[slot];
            changed = true;
        }
    }
    if (changed) {
        SaveToDisk();
    }
}

int RunState::SavedHealth() {
    return g_state.health;
}

int RunState::SavedArmor() {
    return g_state.armor;
}

void RunState::SetSavedVitals(int health, int armor) {
    armor = std::max(0, armor);
    if (g_state.health == health && g_state.armor == armor) {
        return;
    }
    g_state.health = health;
    g_state.armor = armor;
    SaveToDisk();
}

bool RunState::SavedPosition(float* x, float* y, float* z) {
    if (!g_state.hasPosition) {
        return false;
    }
    *x = g_state.posX;
    *y = g_state.posY;
    *z = g_state.posZ;
    return true;
}

void RunState::SetSavedPosition(float x, float y, float z) {
    // Quantize to decimeters before the change check so the periodic mirror
    // does not rewrite the file for sub-perceptible drift.
    const float qx = std::floor(x * 10.0f) / 10.0f;
    const float qy = std::floor(y * 10.0f) / 10.0f;
    const float qz = std::floor(z * 10.0f) / 10.0f;
    if (g_state.hasPosition &&
        g_state.posX == qx && g_state.posY == qy && g_state.posZ == qz) {
        return;
    }
    g_state.hasPosition = true;
    g_state.posX = qx;
    g_state.posY = qy;
    g_state.posZ = qz;
    SaveToDisk();
}

int RunState::CollectedPackages() {
    return g_state.packages;
}

void RunState::SetCollectedPackages(int count) {
    const int clamped = std::max(0, count);
    if (g_state.packages == clamped) {
        return;
    }
    g_state.packages = clamped;
    SaveToDisk();
}

bool RunState::IsPackageCollected(int pointIndex) {
    return g_state.packageIndices.count(pointIndex) != 0;
}

void RunState::MarkPackageCollected(int pointIndex) {
    if (pointIndex < 0) {
        return;
    }
    const auto inserted = g_state.packageIndices.insert(pointIndex);
    if (inserted.second) {
        Logger::Log("RunState: package point %d collected (tracked=%zu)",
                    pointIndex, g_state.packageIndices.size());
        SaveToDisk();
    }
}

int RunState::LastLaunchedMission() {
    return g_lastLaunchedMission;
}

void RunState::SetLastLaunchedMission(int actualMissionIndex) {
    g_lastLaunchedMission = actualMissionIndex;
}

int RunState::PendingValidationMission() {
    return g_pendingValidationMission;
}

const char* RunState::PendingValidationSyntheticMission() {
    return g_pendingValidationKey.empty() ? nullptr : g_pendingValidationKey.c_str();
}

void RunState::SetPendingValidationMission(int actualMissionIndex, const char* syntheticKey) {
    g_pendingValidationMission = actualMissionIndex;
    g_pendingValidationKey = syntheticKey ? syntheticKey : "";
}

void RunState::ClearPendingValidationMission() {
    g_pendingValidationMission = -1;
    g_pendingValidationKey.clear();
}

void RunState::SetRunStartPending(bool pending) {
    g_runStartPending = pending;
}

void RunState::SetSkipIntroOnNextNewGame(bool skip) {
    g_skipIntroOnNextNewGame = skip;
}
