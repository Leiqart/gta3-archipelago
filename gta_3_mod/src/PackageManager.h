#pragma once
#include <functional>

namespace PackageManager {
    // Hidden package milestone and the Archipelago location it checks.
    struct Milestone {
        int packages;
        const char* location;
    };

    // Hidden packages placed in GTA III, as stored in CPlayerInfo::m_nTotalPackages.
    constexpr int kTotalPackages = 100;

    // One check every 10 packages, matching the hidden_packages_<n> locations of data.py.
    inline constexpr Milestone kMilestones[] = {
        {10, "hidden_packages_10"},  {20, "hidden_packages_20"},  {30, "hidden_packages_30"},
        {40, "hidden_packages_40"},  {50, "hidden_packages_50"},  {60, "hidden_packages_60"},
        {70, "hidden_packages_70"},  {80, "hidden_packages_80"},  {90, "hidden_packages_90"},
        {100, "hidden_packages_100"},
    };

    // Receiver of every newly reached milestone.
    using MilestoneSink = void (*)(const Milestone& milestone);

    // Plugs in the milestone receiver; nullptr removes it.
    void SetMilestoneSink(MilestoneSink sink);

    // Forgets reported milestones, for example when another Archipelago room is joined.
    void Reset();

    // Reads CWorld::Players[0].m_nCollectedPackages and reports milestones not sent yet.
    int Update(int collectedPackages);

    // Tells whether the milestone of this package count was already reported.
    bool IsReported(int packages);

    // Runtime detection uses the reward flag, not a second package-count check.
    // A durable check must succeed before vanilla hideout rewards are suppressed.
    bool ConsumeReward(int packages, int& rewardFlag,
                       const std::function<bool(const char*)>& check);
}
