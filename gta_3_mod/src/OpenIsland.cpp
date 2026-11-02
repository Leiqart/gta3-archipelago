#include "OpenIsland.h"
#include "Log.h"

namespace {
    // Classic model IDs and crossing positions; no hardcoded SCM global offsets.
    constexpr int kSubwayGate = 1280, kTunnelGate = 1281, kLiftBarrier = 1299;
    constexpr Opcode::Bounds kCallahanRoad{{619.6f, -911.5f, 45.0f}, {834.3f, -954.5f, 32.0f}};
    constexpr Opcode::Bounds kPortlandTunnel{{659.6f, 200.0f, -20.0f}, {945.8f, 147.5f, 5.0f}};
    constexpr Opcode::Bounds kTunnelJunction{{529.6f, 106.5f, -30.0f}, {581.4f, 65.7f, 0.0f}};
    constexpr Opcode::Bounds kShoresideTunnel{{496.7f, 75.5f, -30.0f}, {484.0f, 44.2f, 0.0f}};
    constexpr Opcode::Bounds kLiftBridgeRoad{{-46.8f, -648.0f, 39.0f}, {-69.1f, -614.0f, 50.0f}};
    struct BridgeRepair { Opcode::Point position; int from, to; };
    constexpr BridgeRepair kBridgeRepairs[] = {
        {{525.362f, -927.066f, 71.841f}, 1140, 1141},
        {{706.432f, -935.82f, 67.071f}, 1140, 1141},
        {{529.023f, -920.098f, 43.504f}, 1147, 1143},
        {{702.763f, -939.963f, 38.736f}, 1147, 1143},
        {{529.023f, -942.94f, 43.504f}, 1146, 1145},
        {{702.764f, -919.963f, 38.736f}, 1146, 1145},
        {{525.362f, -927.066f, 71.841f}, 1187, 1181},
        {{706.432f, -935.82f, 67.071f}, 1187, 1181},
        {{521.146f, -922.94f, 43.504f}, 1185, 1183},
        {{702.763f, -939.963f, 38.736f}, 1185, 1183},
        {{529.023f, -940.098f, 43.504f}, 1186, 1184},
        {{702.764f, -919.963f, 38.736f}, 1186, 1184},
    };
}

bool OpenIsland::Controller::OpenStaunton() {
    for (const auto& repair : kBridgeRepairs)
        if (!Opcode::SwapNearestBuildingModel(executor_, repair.position, 20.0f, repair.from, repair.to)) return false;
    // Intro rubble and the temporary Portland roadworks barrier.
    if (!Opcode::ClearPassage(executor_, {715.746f, -937.908f, 40.194f}, 15.0f, 725) ||
        !Opcode::ClearPassage(executor_, {787.835f, -939.24f, 38.971f}, 15.0f, 724) ||
        !Opcode::SetClosestObjectVisibility(executor_, {1027.26f, -933.796f, 15.042f}, 50.0f, 855, false) ||
        !Opcode::SwapNearestBuildingModel(executor_, {1027.26f, -933.796f, 15.042f}, 50.0f, 855, 218) ||
        !Opcode::ClearPassage(executor_, {988.963f, -471.778f, 5.2f}, 10.0f, kSubwayGate) ||
        !Opcode::ClearPassage(executor_, {730.331f, 172.467f, -21.075f}, 10.0f, kTunnelGate)) return false;
    return Opcode::SwitchRoadsOn(executor_, kCallahanRoad) &&
        Opcode::SwitchRoadsOn(executor_, kPortlandTunnel) &&
        Opcode::SwitchRoadsOn(executor_, kTunnelJunction) && Opcode::SetIndustrialPassed(executor_);
}

bool OpenIsland::Controller::OpenShoreside() {
    return Opcode::ClearPassage(executor_, {-672.0f, -760.0f, 8.3f}, 5.0f, kSubwayGate) &&
        Opcode::ClearPassage(executor_, {-672.0f, -768.9f, 8.3f}, 5.0f, kSubwayGate) &&
        Opcode::ClearPassage(executor_, {533.3f, 97.0f, -21.3f}, 10.0f, kTunnelGate) &&
        Opcode::ClearPassage(executor_, {-73.137f, -630.333f, 25.932f}, 15.0f, kLiftBarrier) &&
        Opcode::SwitchRoadsOn(executor_, kShoresideTunnel) &&
        Opcode::SwitchRoadsOn(executor_, kLiftBridgeRoad) && Opcode::SetCommercialPassed(executor_);
}

void OpenIsland::Controller::Reset() {
    nextRefresh_ = 0;
    lastRequest_ = 0;
    failureReported_ = false;
}

bool OpenIsland::Controller::Update(bool staunton, bool shoreside, bool canModifyWorld, std::uint64_t nowMs) {
    // Shoreside requires a physical route through the preceding crossings.
    // Logical AP mission access still requires the corresponding region's item.
    const int request = shoreside ? 2 : staunton ? 1 : 0;
    if (!canModifyWorld || request == 0) {
        nextRefresh_ = 0;
        if (request == 0) lastRequest_ = 0;
        return true;
    }
    if (request == lastRequest_ && nowMs < nextRefresh_) return true;
    const bool success = OpenStaunton() && (request != 2 || OpenShoreside());
    // Idempotent refresh catches save loads and gates recreated by vanilla
    // scripts, including FRANK4 after an early Shoreside item. Never do this
    // during a mission's object-creation/control sequence.
    nextRefresh_ = nowMs + 1000;
    if (success && (request != lastRequest_ || failureReported_))
        Log::Write("Island crossings opened: %s", request == 2 ? "Staunton and Shoreside" : "Staunton");
    if (!success && !failureReported_) Log::Write("Island opening failed; retrying after a safe game tick");
    failureReported_ = !success;
    lastRequest_ = request;
    return success;
}
