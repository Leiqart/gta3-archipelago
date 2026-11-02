#pragma once
#include "Session.h"
#include "RuntimeData.h"

namespace MissionAccess {
    inline bool CanStart(const Session& session, const std::string& location) {
        if (!session.Ready() || session.Faulted()) return false;
        for (const auto& rule : RuntimeData::kRules) {
            if (location != rule.location) continue;
            const std::string region = rule.region;
            if (region == "Staunton" && !session.HasItem("unlock_commercial") &&
                !session.IsChecked("mission_frank4")) return false;
            if (region == "Shoreside" && !session.HasItem("unlock_suburban") &&
                !session.IsChecked("mission_love3")) return false;
            bool open = !*rule.unlock || session.HasItem(rule.unlock);
            if (*rule.openers[0]) {
                open = *rule.unlock && session.HasItem(rule.unlock);
                for (const auto* opener : rule.openers)
                    if (*opener && session.IsChecked(opener)) open = true;
            }
            if (!open) return false;
            for (const auto* parent : rule.parents)
                if (*parent && !session.IsChecked(parent)) return false;
            return true;
        }
        return false;
    }
}
