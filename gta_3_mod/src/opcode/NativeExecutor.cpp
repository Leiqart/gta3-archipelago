#include "NativeExecutor.h"
#include "../Log.h"
#include "../IntroSequence.h"

#include <array>
#include <cstddef>
#include <cstring>

namespace {
    // GTA III PC 1.0 CRunningScript layout, verified by Plugin-SDK.
    struct ScriptThread {
        void* next;
        void* previous;
        char name[8];
        std::uint32_t ip;
        int stack[6];
        std::uint16_t stackPointer;
        std::uint16_t padding;
        int locals[18]; // 16 variables plus two timers
        bool condition;
        bool missionCleanup;
        bool skipWake;
        std::uint8_t pad7B;
        std::uint32_t wakeTime;
        std::uint16_t logicalOp;
        bool notFlag;
        bool wastedBustedCheck;
        bool wastedBusted;
        bool mission;
        std::uint16_t pad86;
    };
    static_assert(sizeof(ScriptThread) == 0x88);
    static_assert(offsetof(ScriptThread, locals) == 0x30);
    static_assert(offsetof(ScriptThread, condition) == 0x78);
    constexpr std::uintptr_t kScriptSpace = 0x74B248;
    constexpr std::uintptr_t kProcessCommand = 0x439500;
    constexpr std::uintptr_t kScriptParams = 0x6ED460;
}

bool Opcode::IsEightScript(void* script) {
    return script && _strnicmp(static_cast<ScriptThread*>(script)->name, "EIGHT", 8) == 0;
}

bool Opcode::InitializeEightHandles(void* script) {
    auto* thread = static_cast<ScriptThread*>(script);
    if (!IsEightScript(script) || thread->ip != IntroSequence::kMissionBase + IntroSequence::kInitialize)
        return false;
    // Only EIGHT-owned handles, not the world's flags, inventory or other actors.
    constexpr unsigned int handles[] = {6132, 6136, 6140, 6144, 6148, 6180, 6184,
        6192, 6196, 6284, 6288, 6336, 6344};
    for (auto offset : handles) *reinterpret_cast<int*>(kScriptSpace + offset) = -1;
    *reinterpret_cast<int*>(kScriptSpace + 32) = 0; // fresh GML attempt, not the vanilla critical restart
    return true;
}

bool Opcode::EnterLuigiCutscene(void* script, int playerPed, int eightball) {
    auto* thread = static_cast<ScriptThread*>(script);
    if (!IsEightScript(script) || thread->ip != IntroSequence::kMissionBase + IntroSequence::kDirectEntry ||
        thread->stackPointer != 1 || playerPed < 0 || eightball < 0) return false;
    *reinterpret_cast<int*>(kScriptSpace + 44) = playerPed;
    *reinterpret_cast<int*>(kScriptSpace + 6144) = eightball;
    thread->ip = IntroSequence::kMissionBase + IntroSequence::kLuigiCutscene;
    thread->logicalOp = 0;
    thread->notFlag = false;
    return true;
}

bool Opcode::ReturnFromGiveMeLiberty(void* script) {
    auto* thread = static_cast<ScriptThread*>(script);
    if (!IsEightScript(script) || thread->ip != IntroSequence::kMissionBase + IntroSequence::kHideoutFinished ||
        thread->stackPointer != 1 || thread->stack[0] !=
            static_cast<int>(IntroSequence::kMissionBase + IntroSequence::kCleanupReturn)) return false;
    // Return from mission_start_eightball. Its caller runs the original
    // death/arrest test, cleanup subroutine and TERMINATE_THIS_SCRIPT.
    thread->ip = thread->stack[--thread->stackPointer];
    thread->logicalOp = 0;
    thread->notFlag = false;
    return true;
}

int Opcode::EightCarHandle() { return *reinterpret_cast<int*>(kScriptSpace + 6148); }

bool Opcode::AbortLuigiPreparation(void* script) {
    auto* thread = static_cast<ScriptThread*>(script);
    if (!IsEightScript(script) || thread->ip != IntroSequence::kMissionBase + IntroSequence::kDirectEntry ||
        thread->stackPointer != 1 || thread->stack[0] !=
            static_cast<int>(IntroSequence::kMissionBase + IntroSequence::kCleanupReturn)) return false;
    thread->ip = thread->stack[--thread->stackPointer];
    thread->logicalOp = 0;
    thread->notFlag = false;
    return true;
}

bool Opcode::NativeExecutor::Execute(const Command& command, bool* condition) {
    try {
        auto bytes = Encode(command);
        ScriptThread thread{};
        std::memcpy(thread.name, "AP-OP", 5);
        thread.wastedBustedCheck = true;
        // Same offset convention as Plugin-SDK's synchronous ScriptCommands.
        // The bytecode lives in our buffer: no patch or helper thread in main.scm.
        thread.ip = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(bytes.data()) - kScriptSpace);
        std::array<int, 32> savedParams{};
        std::memcpy(savedParams.data(), reinterpret_cast<void*>(kScriptParams), sizeof(savedParams));
        reinterpret_cast<char(__thiscall*)(ScriptThread*)>(kProcessCommand)(&thread);
        std::memcpy(reinterpret_cast<void*>(kScriptParams), savedParams.data(), sizeof(savedParams));
        std::size_t index = 0;
        for (const auto& parameter : command.parameters)
            if (const auto* output = std::get_if<int*>(&parameter)) *(*output) = thread.locals[index++];
        if (condition) *condition = thread.condition;
        return true;
    } catch (const std::exception& error) {
        Log::Write("SCM opcode 0x%04X rejected: %s", static_cast<unsigned int>(command.id), error.what());
        return false;
    }
}

int Opcode::ReadIntegerParameter(void* script, std::uint32_t ip) {
    Parameters parameters;
    ReadParameters(script, ip, 1, parameters);
    return parameters.values[0];
}

float Opcode::Parameters::Float(std::size_t index) const {
    float value = 0;
    std::memcpy(&value, &values.at(index), sizeof(value));
    return value;
}

bool Opcode::ReadParameters(void* script, std::uint32_t ip, short count, Parameters& result) {
    if (!script || count < 0 || count > 32) return false;
    std::array<int, 32> previous{};
    std::memcpy(previous.data(), reinterpret_cast<void*>(kScriptParams), sizeof(previous));
    reinterpret_cast<void(__thiscall*)(void*, unsigned int*, short)>(0x4382E0)(script, &ip, count);
    std::memcpy(result.values.data(), reinterpret_cast<void*>(kScriptParams), sizeof(result.values));
    std::memcpy(reinterpret_cast<void*>(kScriptParams), previous.data(), sizeof(previous));
    result.nextIp = ip;
    return true;
}

bool Opcode::ReadOutputVariable(void* script, std::uint32_t ip, int& result) {
    constexpr std::uint32_t size = 163840;
    if (!script || ip > size - 3) return false;
    const auto* argument = reinterpret_cast<const std::uint8_t*>(kScriptSpace + ip);
    const unsigned int index = argument[1] | (argument[2] << 8);
    if (argument[0] == 2 && index <= size - sizeof(int)) {
        std::memcpy(&result, reinterpret_cast<void*>(kScriptSpace + index), sizeof(result));
        return true;
    }
    if (argument[0] == 3 && index < 18) {
        result = static_cast<ScriptThread*>(script)->locals[index];
        return true;
    }
    return false;
}

void Opcode::SetCompareResult(void* script, bool value) { static_cast<ScriptThread*>(script)->condition = value; }
