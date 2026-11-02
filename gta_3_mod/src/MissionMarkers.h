#pragma once
#include "MissionAccess.h"
#include "MissionTable.h"
#include "opcode/Wrappers.h"
#include <array>
#include <cstdint>
#include <vector>

namespace MissionMarkers {
    struct Contact { const char* chain; Opcode::Point position; int sprite; };
    inline constexpr Contact kContacts[] = {
        {"luigi", {892.8f, -425.8f, 13.9f}, 13},
        {"joey", {1191.7f, -870.0f, 15.0f}, 10},
        {"toni", {1219.8f, -319.7f, 27.4f}, 19},
        {"frankie", {1455.7f, -187.3f, 55.6f}, 16},
        {"diablo", {938.4f, -230.5f, 15.0f}, 8},
        {"asuka", {523.6f, -639.4f, 16.6f}, 1},
        {"kenji", {459.1f, -1413.0f, 26.1f}, 11},
        {"ray", {38.8f, -725.5f, 22.8f}, 15},
        {"donald_love", {86.1f, -1548.7f, 28.2f}, 6},
        {"yardie", {120.7f, -272.1f, 16.1f}, 12},
        {"catalina", {-362.8f, 245.9f, 60.0f}, 3},
        {"asuka_suburban", {366.94f, -328.03f, 20.27f}, 1},
        {"hood", {-443.5f, -6.1f, 3.8f}, 9},
        {"meat_factory", {1224.6f, -840.3f, 15.0f}, 4},
        {"intro", {811.875f, -939.9375f, 36.75f}, 13}, // Vanilla bridge trigger, never an AP marker.
    };
    constexpr std::size_t kContactCount = sizeof(kContacts) / sizeof(kContacts[0]);

    const Contact* ContactAt(Opcode::Point position);
    const MissionTable::Entry* PlayableMission(const Session& session, const Contact& contact);
    bool OwnsMission(int mission);
    short LocateParameterCount(std::uint16_t opcode);
    bool SuppressVanillaLocate(std::uint32_t ip, bool missionScript, std::uint16_t rawOpcode, Opcode::Point target);

    struct Frame {
        Opcode::Point position{};
        bool worldReady = false;
        bool missionRunning = false;
        bool onFoot = false;
        bool canStart = false;
        bool fading = false;
        bool paused = false;
    };

    class Controller {
    public:
        explicit Controller(Opcode::Executor& executor);
        // Returns the newly launched mission ID, or -1. Does not emit AP checks.
        int Update(const Session& session, const Frame& frame, std::uint64_t gameTimeMs);
        void Render(const Session& session, const Frame& frame);
        bool Busy() const { return phase_ != Phase::Idle; }
        // A world restart invalidates handles: forget, never remove stale IDs.
        void Reset();
        void TrackVanillaBlip(int handle);
        bool IsVanillaBlip(int handle) const;
        void VanillaBlipChanged(int handle);
    private:
        enum class Phase { Idle, Fading, AwaitingScript };
        void ClearOwnedBlips();
        void ReconcileBlips(const Session& session);
        void Cancel(bool restorePlayer);
        bool NearContact(const Opcode::Point& position, int contact) const;
        Opcode::Executor& executor_;
        std::array<int, kContactCount> blips_{};
        struct VanillaBlip { int handle; bool hidden; };
        std::vector<VanillaBlip> vanillaBlips_;
        Phase phase_ = Phase::Idle;
        int pending_ = -1;
        int leaveContact_ = -1;
        bool missionSeen_ = false;
        std::uint64_t phaseStarted_ = 0;
    };
}
