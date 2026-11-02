#pragma once
#include "opcode/Wrappers.h"
#include <cstdint>

namespace OpenIsland {
    // Derived from AP ownership/story checks on every tick; never advance a
    // mission flag or emit a check as a side effect of opening a crossing.
    class Controller {
    public:
        explicit Controller(Opcode::Executor& executor) : executor_(executor) {}
        bool Update(bool staunton, bool shoreside, bool canModifyWorld, std::uint64_t nowMs);
        void Reset();
    private:
        bool OpenStaunton();
        bool OpenShoreside();
        Opcode::Executor& executor_;
        std::uint64_t nextRefresh_ = 0;
        int lastRequest_ = 0;
        bool failureReported_ = false;
    };
}
