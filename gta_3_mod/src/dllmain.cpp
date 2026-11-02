#include <windows.h>

#include "Connection.h"
#include "Diagnostics.h"
#include "MissionManager.h"
#include "GameRuntime.h"
#include "Log.h"
#include <atomic>
#include <cstring>

namespace {
    HANDLE g_initialization = nullptr;
    HMODULE g_lifetime = nullptr;
    std::atomic<bool> g_shutdown{false};

    DWORD WINAPI Initialize(void* parameter) {
        const auto module = static_cast<HMODULE>(parameter);
        Log::Open(module);
        Diagnostics::Install();
        char executable[MAX_PATH]{};
        GetModuleFileNameA(nullptr, executable, MAX_PATH);
        const auto* name = strrchr(executable, '\\');
        name = name ? name + 1 : executable;
        const bool host = _stricmp(name, "AsiHost.exe") == 0;
        if (!g_shutdown.load() && (host || GameRuntime::Install(module)))
            Connection::Start(module);
        return 0;
    }
}

// Explicit shutdown runs outside loader lock. Keep a module reference while
// the worker and hooks are active, even if another mod calls FreeLibrary early.
extern "C" __declspec(dllexport) void __cdecl GTA3AP_Shutdown() {
    g_shutdown.store(true);
    if (g_initialization) {
        WaitForSingleObject(g_initialization, INFINITE);
        CloseHandle(g_initialization);
        g_initialization = nullptr;
    }
    GameRuntime::Shutdown();
    Connection::Stop(false);
    Diagnostics::Uninstall();
    Log::Close();
    if (g_lifetime) {
        const auto held = g_lifetime;
        g_lifetime = nullptr;
        FreeLibrary(held);
    }
}

// Entry point used by AsiHost to test the lifecycle without GTA III memory.
extern "C" __declspec(dllexport) BOOL __cdecl GTA3AP_RunMissionLifecycleSelfTest() {
    if (g_initialization) WaitForSingleObject(g_initialization, INFINITE);
    return MissionManager::RunLifecycleSelfTest() ? TRUE : FALSE;
}

// Starts and stops the connection service with the ASI lifetime.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCSTR>(&Initialize), &g_lifetime)) return FALSE;
            g_initialization = CreateThread(nullptr, 0, Initialize, module, 0, nullptr);
            return g_initialization != nullptr;

        case DLL_PROCESS_DETACH:
            // Never wait for another thread from DllMain.
            if (reserved != nullptr) {
                Diagnostics::LogProcessExit();
                Connection::Stop(true);
            }
            break;
    }
    return TRUE;
}
