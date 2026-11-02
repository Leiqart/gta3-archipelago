#include "MissionMarkers.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    float Distance(Opcode::Point first, Opcode::Point second) {
        return std::hypot(first.x - second.x, first.y - second.y);
    }
    const RuntimeData::Rule* RuleFor(const char* location) {
        for (const auto& rule : RuntimeData::kRules)
            if (std::strcmp(rule.location, location) == 0) return &rule;
        return nullptr;
    }
}

const MissionMarkers::Contact* MissionMarkers::ContactAt(Opcode::Point position) {
    for (const auto& contact : kContacts)
        if (Distance(contact.position, position) <= 2.5f) return &contact;
    return nullptr;
}

const MissionTable::Entry* MissionMarkers::PlayableMission(const Session& session, const Contact& contact) {
    // Give Me Liberty keeps its vanilla bridge trigger and critical restart: no AP marker there.
    if (std::strcmp(contact.chain, "intro") == 0) return nullptr;
    for (const auto& mission : MissionTable::kEntries) {
        const auto* rule = RuleFor(mission.location);
        if (rule && std::strcmp(rule->chain, contact.chain) == 0 &&
            !session.IsChecked(mission.location) && MissionAccess::CanStart(session, mission.location)) return &mission;
    }
    return nullptr;
}

bool MissionMarkers::OwnsMission(int mission) {
    const auto* location = MissionTable::LocationFor(mission);
    const auto* rule = location ? RuleFor(location) : nullptr;
    if (rule) for (const auto& contact : kContacts)
        if (std::strcmp(rule->chain, contact.chain) == 0) return true;
    return false;
}

short MissionMarkers::LocateParameterCount(std::uint16_t opcode) {
    opcode &= 0x7FFF;
    if (opcode >= 0x00E3 && opcode <= 0x00E8) return 6;
    if (opcode >= 0x00F5 && opcode <= 0x00FA) return 8;
    return 0;
}

bool MissionMarkers::SuppressVanillaLocate(std::uint32_t ip, bool missionScript,
                                          std::uint16_t rawOpcode, Opcode::Point target) {
    // Positive contact proximity tests gate the fade/control setup. Do not touch
    // mission objectives, negative leave-marker tests or unrelated side jobs.
    return ip < 128 * 1024 && !missionScript && !(rawOpcode & 0x8000) &&
        LocateParameterCount(rawOpcode) != 0 && ContactAt(target) != nullptr;
}

MissionMarkers::Controller::Controller(Opcode::Executor& executor) : executor_(executor) { Reset(); }

void MissionMarkers::Controller::Reset() {
    blips_.fill(-1);
    vanillaBlips_.clear();
    phase_ = Phase::Idle;
    pending_ = leaveContact_ = -1;
    missionSeen_ = false;
    phaseStarted_ = 0;
}

void MissionMarkers::Controller::TrackVanillaBlip(int handle) {
    if (handle < 0 || IsVanillaBlip(handle) || vanillaBlips_.size() >= 256) return;
    vanillaBlips_.push_back({handle, false});
}

bool MissionMarkers::Controller::IsVanillaBlip(int handle) const {
    return std::any_of(vanillaBlips_.begin(), vanillaBlips_.end(),
        [&](const VanillaBlip& blip) { return blip.handle == handle; });
}

void MissionMarkers::Controller::VanillaBlipChanged(int handle) {
    for (auto& blip : vanillaBlips_) if (blip.handle == handle) blip.hidden = false;
}

void MissionMarkers::Controller::ClearOwnedBlips() {
    for (auto& blip : blips_) {
        if (blip >= 0 && Opcode::RemoveBlip(executor_, blip)) blip = -1;
    }
}

void MissionMarkers::Controller::ReconcileBlips(const Session& session) {
    for (auto& blip : vanillaBlips_)
        if (!blip.hidden && Opcode::HideBlip(executor_, blip.handle)) blip.hidden = true;
    for (std::size_t i = 0; i < kContactCount; ++i) {
        const bool wanted = PlayableMission(session, kContacts[i]) != nullptr;
        if (wanted && blips_[i] < 0)
            Opcode::AddCoordinateBlip(executor_, kContacts[i].position, kContacts[i].sprite, blips_[i]);
        else if (!wanted && blips_[i] >= 0 && Opcode::RemoveBlip(executor_, blips_[i])) blips_[i] = -1;
    }
}

void MissionMarkers::Controller::Cancel(bool restorePlayer) {
    if (restorePlayer) Opcode::RestoreFreeRoam(executor_, 0);
    phase_ = Phase::Idle;
    pending_ = -1;
}

bool MissionMarkers::Controller::NearContact(const Opcode::Point& position, int contact) const {
    return Distance(position, kContacts[contact].position) <= 4.0f &&
        std::fabs(position.z - kContacts[contact].position.z) <= 4.0f;
}

int MissionMarkers::Controller::Update(const Session& session, const Frame& frame, std::uint64_t now) {
    if (frame.missionRunning) missionSeen_ = true;
    if (frame.paused) return -1;
    if (!frame.worldReady) {
        if (Busy()) Cancel(false); // Death/respawn and world reload own their camera.
        return -1;
    }
    if (missionSeen_ && !frame.missionRunning) {
        // A mission ending on a contact, like a failed Give Me Liberty launched by the intro, needs the player to leave first.
        missionSeen_ = false;
        for (int i = 0; leaveContact_ < 0 && i < static_cast<int>(kContactCount); ++i)
            if (NearContact(frame.position, i)) leaveContact_ = i;
    }
    if (leaveContact_ >= 0 && !frame.missionRunning && !Busy() && frame.canStart &&
        !NearContact(frame.position, leaveContact_)) leaveContact_ = -1;
    if (frame.missionRunning) {
        ClearOwnedBlips();
        // An observed real script takes ownership of controls and fades.
        if (Busy()) Cancel(false);
        return -1;
    }
    if (!session.Ready() || session.Faulted()) {
        if (Busy()) Cancel(true);
        ClearOwnedBlips();
        return -1;
    }
    if (phase_ == Phase::Fading) {
        const auto* location = MissionTable::LocationFor(pending_);
        if (!location || session.IsChecked(location) || !MissionAccess::CanStart(session, location)) {
            Cancel(true); return -1;
        }
        if (now < phaseStarted_ || now - phaseStarted_ > 5000) {
            Log::Write("Mission marker fade timed out; restoring player control");
            Cancel(true); return -1;
        }
        if (frame.fading || now - phaseStarted_ < 500) return -1;
        if (!Opcode::LaunchMission(executor_, pending_)) {
            Cancel(true); return -1;
        }
        phase_ = Phase::AwaitingScript;
        phaseStarted_ = now;
        Log::Write("AP contact marker launched mission %d", pending_);
        return pending_;
    }
    if (phase_ == Phase::AwaitingScript) {
        const auto* location = MissionTable::LocationFor(pending_);
        if (location && session.IsChecked(location)) Cancel(false);
        else if (now < phaseStarted_ || now - phaseStarted_ > 5000) {
            Log::Write("Mission script did not start; restoring player control");
            Cancel(true);
        }
        return -1;
    }
    ReconcileBlips(session);
    if (!frame.onFoot || !frame.canStart || frame.fading) return -1;
    for (std::size_t i = 0; i < kContactCount; ++i) {
        if (static_cast<int>(i) == leaveContact_ || Distance(frame.position, kContacts[i].position) > 1.5f ||
            std::fabs(frame.position.z - kContacts[i].position.z) > 2.5f) continue;
        const auto* mission = PlayableMission(session, kContacts[i]);
        if (!mission) continue;
        pending_ = mission->missionId;
        leaveContact_ = static_cast<int>(i);
        ClearOwnedBlips();
        Log::Write("AP contact %s: preparing mission %d", kContacts[i].chain, pending_);
        if (!Opcode::BeginMissionFade(executor_, 0)) { Cancel(true); return -1; }
        phase_ = Phase::Fading;
        phaseStarted_ = now;
        return -1;
    }
    return -1;
}

void MissionMarkers::Controller::Render(const Session& session, const Frame& frame) {
    // Vanilla scripts can create/re-show contact icons during this frame.
    // Hide them after script processing, while leaving mission objectives alone.
    if (frame.worldReady && !frame.paused && !frame.missionRunning)
        for (auto& blip : vanillaBlips_)
            if (!blip.hidden && Opcode::HideBlip(executor_, blip.handle)) blip.hidden = true;
    if (Busy() || !frame.worldReady || frame.paused || frame.missionRunning || !session.Ready() || session.Faulted()) return;
    for (const auto& contact : kContacts)
        if (Distance(frame.position, contact.position) < 100.0f && PlayableMission(session, contact))
            Opcode::DrawMissionMarker(executor_, contact.position);
}
