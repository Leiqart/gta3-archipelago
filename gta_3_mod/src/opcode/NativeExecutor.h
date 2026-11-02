#pragma once
#include "Command.h"
#include <array>

namespace Opcode {
    // Construct/use only after GameRuntime verifies the GTA III 1.0 executable.
    // Calls must be synchronous on the game thread.
    class NativeExecutor final : public Executor {
    public:
        bool Execute(const Command& command, bool* condition = nullptr) override;
    };
    int ReadIntegerParameter(void* script, std::uint32_t ip);
    struct Parameters {
        std::uint32_t nextIp = 0;
        std::array<int, 32> values{};
        float Float(std::size_t index) const;
    };
    bool ReadParameters(void* script, std::uint32_t ip, short count, Parameters& result);
    bool ReadOutputVariable(void* script, std::uint32_t ip, int& result);
    void SetCompareResult(void* script, bool value);
    // Narrow adapters for the fingerprint-verified vanilla EIGHT layout.
    bool IsEightScript(void* script);
    bool InitializeEightHandles(void* script);
    bool EnterLuigiCutscene(void* script, int playerPed, int eightball);
    bool ReturnFromGiveMeLiberty(void* script);
    bool AbortLuigiPreparation(void* script);
    int EightCarHandle();
}
