#pragma once
#include "MissionManager.h"
#include "Session.h"

namespace MissionProgress {
    // Shared by the real game callback and the automated integration scenario.
    void Record(Session& session, MissionManager::Event event, int mission);
}
