#include "Diagnostics.h"

#include <windows.h>

#include "Log.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {
    constexpr long kMaxFaults = 20;
    constexpr USHORT kMaxFrames = 24;

    PVOID g_handler = nullptr;
    std::atomic<long> g_faults{0};

    // Describes a code address as module+offset so it can be matched against gta3.exe or the ASI.
    void Describe(const void* address, char* text, std::size_t size) {
        HMODULE module = nullptr;
        char path[MAX_PATH] = {};
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(address), &module) &&
            GetModuleFileNameA(module, path, MAX_PATH)) {
            const char* name = std::strrchr(path, '\\');
            std::snprintf(text, size, "%s+0x%X", name ? name + 1 : path,
                          static_cast<unsigned>(static_cast<const char*>(address) - reinterpret_cast<const char*>(module)));
        } else {
            std::snprintf(text, size, "0x%p", address);
        }
    }

    // Ignores the exceptions Windows and the CRT raise during normal operation.
    bool Benign(DWORD code) {
        return code == 0xE06D7363 || code == 0x406D1388 || code == DBG_PRINTEXCEPTION_C ||
               code == 0x4001000A || code == EXCEPTION_BREAKPOINT || code == DBG_CONTROL_C;
    }

    // Logs the fault and lets the game or Windows handle it as before.
    LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
        const auto* record = info->ExceptionRecord;
        if (Benign(record->ExceptionCode) || g_faults.fetch_add(1) >= kMaxFaults) return EXCEPTION_CONTINUE_SEARCH;
        char where[MAX_PATH + 32];
        Describe(record->ExceptionAddress, where, sizeof(where));
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            Log::Write("Fault 0x%08lX at %s: %s 0x%p (thread %lu)", record->ExceptionCode, where,
                       record->ExceptionInformation[0] ? "write" : "read",
                       reinterpret_cast<void*>(record->ExceptionInformation[1]), GetCurrentThreadId());
        } else {
            Log::Write("Fault 0x%08lX at %s (thread %lu)", record->ExceptionCode, where, GetCurrentThreadId());
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void Diagnostics::Install() {
    if (!g_handler) g_handler = AddVectoredExceptionHandler(1, OnException);
}

void Diagnostics::Uninstall() {
    if (g_handler) {
        RemoveVectoredExceptionHandler(g_handler);
        g_handler = nullptr;
    }
}

void Diagnostics::LogProcessExit() {
    void* frames[kMaxFrames] = {};
    const USHORT count = CaptureStackBackTrace(0, kMaxFrames, frames, nullptr);
    Log::Write("Process exiting on thread %lu; call chain:", GetCurrentThreadId());
    for (USHORT index = 0; index < count; ++index) {
        char where[MAX_PATH + 32];
        Describe(frames[index], where, sizeof(where));
        Log::Write("  #%u %s", static_cast<unsigned>(index), where);
    }
}
