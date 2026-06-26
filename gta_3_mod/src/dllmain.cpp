#include <windows.h>

#include "ApBridge.h"
#include "ApState.h"
#include "Config.h"
#include "DebugKeys.h"
#include "Hooks.h"
#include "Logger.h"
#include "MenuPatch.h"
#include "PluginPaths.h"
#include "RunState.h"

namespace {
    LONG WINAPI CrashLogger(EXCEPTION_POINTERS* info) {
        if (info && info->ExceptionRecord && info->ContextRecord) {
            const EXCEPTION_RECORD& rec = *info->ExceptionRecord;
            const CONTEXT& ctx = *info->ContextRecord;
            Logger::Log("=== UNHANDLED EXCEPTION ===");
            Logger::Log("  code=0x%08lX flags=0x%08lX addr=0x%p",
                        rec.ExceptionCode, rec.ExceptionFlags, rec.ExceptionAddress);
            if (rec.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec.NumberParameters >= 2) {
                Logger::Log("  access %s addr=0x%p",
                            rec.ExceptionInformation[0] == 0 ? "READ"
                                : (rec.ExceptionInformation[0] == 1 ? "WRITE" : "EXEC"),
                            reinterpret_cast<void*>(rec.ExceptionInformation[1]));
            }
            Logger::Log("  EIP=%08lX ESP=%08lX EBP=%08lX EFLAGS=%08lX",
                        ctx.Eip, ctx.Esp, ctx.Ebp, ctx.EFlags);
            Logger::Log("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX",
                        ctx.Eax, ctx.Ebx, ctx.Ecx, ctx.Edx);
            Logger::Log("  ESI=%08lX EDI=%08lX",
                        ctx.Esi, ctx.Edi);
            // Walk the first ~10 dwords of the stack as a poor-man's
            // backtrace — values that look like 0x004xxxxx are likely
            // return addresses inside gta3.exe and pinpoint the call
            // chain leading to the crash.
            auto* sp = reinterpret_cast<DWORD*>(ctx.Esp);
            __try {
                for (int i = 0; i < 10; ++i) {
                    Logger::Log("  [esp+%02d] = %08lX", i * 4, sp[i]);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("  (stack walk faulted at offset %d)", 0);
            }
            Logger::Log("=== END EXCEPTION ===");
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void OnAttach(HMODULE hModule) {
        PluginPaths::Init(hModule);
        Logger::Init(PluginPaths::InModuleDir("III.MissionSelector.log"));
        Logger::Log("MissionSelector ASI loaded (hModule=%p)", hModule);
        // Install crash logger BEFORE anything else so it catches early
        // exceptions too. Returns CONTINUE_SEARCH so Windows still gets
        // to terminate the process as normal — we just want the log.
        SetUnhandledExceptionFilter(CrashLogger);
        Config::Init();
        Config::InstallConfiguredMainScript();
        RunState::Init();
        ApState::Init();
        ApBridge::Init();
        DebugKeys::Init();

        if (!Hooks::Init()) {
            Logger::Log("Hooks::Init failed - running without hooks");
        }
        if (!MenuPatch::Apply()) {
            Logger::Log("MenuPatch::Apply failed");
        }
    }

    void OnDetach() {
        Hooks::Shutdown();
        Logger::Log("MissionSelector ASI unloading");
        Logger::Shutdown();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            OnAttach(hModule);
            break;
        case DLL_PROCESS_DETACH:
            OnDetach();
            break;
    }
    return TRUE;
}
