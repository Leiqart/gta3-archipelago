#include "Session.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "Log.h"
#include "MissionTable.h"
#include "PackageManager.h"
#include "RuntimeData.h"

namespace {
    bool ValidItem(const std::string& value) {
        for (const auto* item : RuntimeData::kItems) if (value == item) return true;
        return false;
    }
    bool ValidLocation(const std::string& value) {
        if (value == "mission_give_me_liberty") return true;
        for (const auto& entry : MissionTable::kEntries)
            if (value == entry.location) return true;
        for (const auto& entry : PackageManager::kMilestones)
            if (value == entry.location) return true;
        return false;
    }
}

Session::Session(Sender sender, std::string directory)
    : sender_(std::move(sender)), directory_(std::move(directory)) {}

std::string Session::StatePath() const {
    // Hash the encoded pair, never use server-provided names as path components.
    const std::string identity = Json::array({seed_, slot_}).dump();
    unsigned long long hash = 14695981039346656037ull;
    for (unsigned char byte : identity) { hash ^= byte; hash *= 1099511628211ull; }
    std::ostringstream name;
    name << "room-" << std::hex << hash << ".json";
    return (std::filesystem::path(directory_) / name.str()).string();
}

bool Session::Persist() {
    if (directory_.empty()) return true; // In-memory host/tests.
    try {
        std::filesystem::create_directories(directory_);
        const auto path = StatePath();
        const Json state = {{"version", 1}, {"seed", seed_}, {"slot", slot_},
            {"items", items_}, {"applied", applied_}, {"checks", checked_}, {"goal", goal_}};
        {
            std::ofstream file(path + ".tmp", std::ios::binary | std::ios::trunc);
            file << state.dump();
            file.flush();
            if (!file) throw std::runtime_error("Cannot write room journal");
        }
        if (!MoveFileExA((path + ".tmp").c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace room journal");
        return true;
    } catch (const std::exception& error) {
        Log::Write("Archipelago persistence failed: %s", error.what());
        faulted_ = true;
        ready_ = false;
        return false;
    }
}

bool Session::Load() {
    if (directory_.empty() || !std::filesystem::exists(StatePath())) return true;
    std::ifstream file(StatePath(), std::ios::binary);
    const Json state = Json::parse(file);
    if (state.at("version") != 1 || state.at("seed") != seed_ || state.at("slot") != slot_)
        return false;
    auto items = state.at("items").get<std::vector<std::string>>();
    for (const auto& item : items) if (!ValidItem(item)) return false;
    const auto applied = state.at("applied").get<std::size_t>();
    if (applied > items.size() || items.size() > 10000) return false;
    items_ = std::move(items);
    applied_ = applied;
    checked_ = state.at("checks").get<std::set<std::string>>();
    for (const auto& location : checked_) if (!ValidLocation(location)) return false;
    goal_ = state.value("goal", false);
    return true;
}

void Session::Disconnected() {
    connected_ = syncing_ = ready_ = false;
    stagedItems_.clear();
    stagedChecks_.clear();
    acknowledged_.clear();
}

bool Session::Receive(const Json& message) {
    try {
        const auto type = message.at("type").get<std::string>();
        if (type == "status") {
            connected_ = message.at("server_connected").get<bool>();
            if (!connected_) Disconnected();
            return true;
        }
        if (type == "room") {
            const auto seed = message.at("seed").get<std::string>();
            const auto slot = message.at("slot").get<std::string>();
            if (seed.empty() || slot.empty()) return false;
            // A live save cannot be silently rebound to another room.
            if ((!seed_.empty() && seed != seed_) || (!slot_.empty() && slot != slot_)) {
                Log::Write("Room changed: restart GTA III with the matching save before continuing");
                faulted_ = true;
                ready_ = false;
                return false;
            }
            if (seed_.empty()) {
                seed_ = seed; slot_ = slot;
                if (!Load()) { faulted_ = true; return false; }
            }
            return true;
        }
        if (type == "sync_start") {
            if (!connected_ || seed_.empty() || faulted_) return false;
            syncing_ = true; ready_ = false;
            stagedItems_.clear(); stagedChecks_.clear(); acknowledged_.clear();
            return true;
        }
        if (type == "item") {
            if (seed_.empty() || faulted_ || (!syncing_ && !ready_)) return false;
            const auto index = message.at("index").get<int>();
            const auto item = message.at("item").get<std::string>();
            if (index < 0 || index >= 10000 || !ValidItem(item)) return false;
            auto& items = syncing_ ? stagedItems_ : items_;
            const auto position = static_cast<std::size_t>(index);
            if (position > items.size()) return false;
            if (position < items.size()) return items[position] == item;
            items.push_back(item);
            return syncing_ || Persist();
        }
        if (type == "checked") {
            const auto location = message.at("location").get<std::string>();
            if (seed_.empty() || !ValidLocation(location)) return false;
            if (syncing_) stagedChecks_.insert(location);
            else {
                acknowledged_.insert(location);
                checked_.insert(location);
                return Persist();
            }
            return true;
        }
        if (type == "sync_end") {
            if (!syncing_ || faulted_) return false;
            // A truncated or changed item history must never replay consumables.
            if (stagedItems_.size() < items_.size() ||
                !std::equal(items_.begin(), items_.end(), stagedItems_.begin())) return false;
            items_ = std::move(stagedItems_);
            acknowledged_ = std::move(stagedChecks_);
            checked_.insert(acknowledged_.begin(), acknowledged_.end());
            syncing_ = false;
            ready_ = connected_;
            if (!Persist()) return false;
            Replay();
            return true;
        }
        // An acknowledgement means queued by the client, not persisted by AP.
        // Only an authoritative checked snapshot suppresses future replay.
        if (type == "check_ack" || type == "goal_ack" || type == "pong" ||
            type == "welcome" || type == "error") return true;
        return false;
    } catch (const std::exception& error) {
        Log::Write("Rejected Archipelago message: %s", error.what());
        faulted_ = true;
        ready_ = false;
        return false;
    }
}

void Session::Replay() {
    if (!ready_ || faulted_) return;
    for (const auto& location : checked_)
        if (!acknowledged_.count(location)) sender_({{"type", "check"}, {"location", location}});
    if (goal_) sender_({{"type", "goal"}});
}

bool Session::Check(const std::string& location) {
    if (seed_.empty() || faulted_ || !ValidLocation(location)) return false;
    if (!checked_.insert(location).second) return true;
    if (location == "mission_cat1") goal_ = true;
    if (!Persist()) return false;
    if (ready_) {
        sender_({{"type", "check"}, {"location", location}});
        if (goal_) sender_({{"type", "goal"}});
    }
    return true;
}

void Session::Tick(const ItemSink& applyItem) {
    if (!ready_ || faulted_) return;
    while (applied_ < items_.size()) {
        if (!applyItem(items_[applied_])) break;
        ++applied_;
        if (!Persist()) break;
    }
}

bool Session::HasItem(const std::string& item) const {
    return std::find(items_.begin(), items_.begin() + applied_, item) != items_.begin() + applied_;
}

bool Session::IsChecked(const std::string& location) const { return checked_.count(location) != 0; }
