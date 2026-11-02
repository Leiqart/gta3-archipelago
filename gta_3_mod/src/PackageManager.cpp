#include "PackageManager.h"

#include "Log.h"

namespace {
    constexpr int kMilestoneCount =
        static_cast<int>(sizeof(PackageManager::kMilestones) / sizeof(PackageManager::kMilestones[0]));

    PackageManager::MilestoneSink g_milestoneSink = nullptr;
    bool g_reported[kMilestoneCount] = {};
}

// Replaces the milestone receiver, for example with the fake game used by tests.
void PackageManager::SetMilestoneSink(MilestoneSink sink) {
    g_milestoneSink = sink;
}

// A new room must receive every milestone again, even those already reached.
void PackageManager::Reset() {
    for (bool& reported : g_reported) {
        reported = false;
    }
}

// Reports each reached milestone once; a lower count from an older save sends nothing.
int PackageManager::Update(int collectedPackages) {
    if (collectedPackages < 0 || collectedPackages > kTotalPackages) {
        return 0;
    }
    int newlyReported = 0;
    for (int index = 0; index < kMilestoneCount; ++index) {
        const Milestone& milestone = kMilestones[index];
        if (collectedPackages < milestone.packages || g_reported[index]) {
            continue;
        }
        g_reported[index] = true;
        ++newlyReported;
        Log::Write("Paquets %d/%d: palier %s atteint", collectedPackages, kTotalPackages,
                   milestone.location);
        if (g_milestoneSink) {
            g_milestoneSink(milestone);
        }
    }
    return newlyReported;
}

// Looks the milestone up by its package count.
bool PackageManager::IsReported(int packages) {
    for (int index = 0; index < kMilestoneCount; ++index) {
        if (kMilestones[index].packages == packages) {
            return g_reported[index];
        }
    }
    return false;
}

bool PackageManager::ConsumeReward(int packages, int& rewardFlag,
                                 const std::function<bool(const char*)>& check) {
    if (rewardFlag != 1) return false;
    for (const auto& milestone : kMilestones) {
        if (milestone.packages != packages) continue;
        if (!check(milestone.location)) return false;
        rewardFlag = 0;
        return true;
    }
    return false;
}
