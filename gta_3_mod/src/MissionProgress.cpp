#include "MissionProgress.h"
#include "MissionTable.h"

void MissionProgress::Record(Session& session, MissionManager::Event event, int mission) {
    if (event != MissionManager::Event::Success) return;
    const char* location = MissionTable::LocationFor(mission);
    if (location) session.Check(location);
}
