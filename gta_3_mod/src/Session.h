#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>
#include "../third_party/nlohmann/json.hpp"

// Owned exclusively by the game thread. Network callbacks only enqueue messages.
class Session {
public:
    using Json = nlohmann::json;
    using Sender = std::function<void(const Json&)>;
    using ItemSink = std::function<bool(const std::string&)>;

    explicit Session(Sender sender, std::string directory = {});
    bool Receive(const Json& message);
    void Disconnected();
    void Tick(const ItemSink& applyItem);
    bool Check(const std::string& location);
    bool HasItem(const std::string& item) const;
    bool IsChecked(const std::string& location) const;
    bool Ready() const { return ready_; }
    bool Faulted() const { return faulted_; }
    std::size_t AppliedCount() const { return applied_; }
    const std::string& Seed() const { return seed_; }
    const std::string& Slot() const { return slot_; }

private:
    bool Persist();
    bool Load();
    std::string StatePath() const;
    void Replay();
    Sender sender_;
    std::string directory_, seed_, slot_;
    bool connected_ = false, syncing_ = false, ready_ = false, faulted_ = false;
    bool goal_ = false;
    std::size_t applied_ = 0;
    std::vector<std::string> items_, stagedItems_;
    std::set<std::string> checked_, acknowledged_, stagedChecks_;
};
