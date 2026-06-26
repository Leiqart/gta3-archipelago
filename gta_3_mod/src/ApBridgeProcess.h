#pragma once

#include <windows.h>

#include <cstdio>
#include <string>

#include "ApConfig.h"
#include "Logger.h"
#include "PluginPaths.h"

namespace ApBridgeProcess {
    constexpr char kExecutableName[] = "GTA3_AP.exe";

    inline bool IsUsableFile(const std::string& path) {
        const DWORD attrs = GetFileAttributesA(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES &&
               (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    inline std::string ResolveExecutablePath() {
        const std::string inGame = PluginPaths::InGameDir(kExecutableName);
        if (IsUsableFile(inGame)) {
            return inGame;
        }

        const std::string inModule = PluginPaths::InModuleDir(kExecutableName);
        if (IsUsableFile(inModule)) {
            return inModule;
        }

        // Dev fallback: launch the freshly built bridge directly from the repo.
        const std::string devBuild =
            PluginPaths::InGameDir("..\\gta3-mission-selector\\output\\Release\\GTA3_AP.exe");
        if (IsUsableFile(devBuild)) {
            return devBuild;
        }

        return std::string();
    }

    inline unsigned int HashLower(const std::string& value) {
        unsigned int hash = 2166136261u;
        for (unsigned char c : value) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<unsigned char>(c - 'A' + 'a');
            }
            hash ^= c;
            hash *= 16777619u;
        }
        return hash;
    }

    inline std::string InstanceMutexNameForGameDir(const std::string& gameDir) {
        char buf[64] = {};
        std::snprintf(buf, sizeof(buf), "Local\\GTA3_AP.%08X", HashLower(gameDir));
        return buf;
    }

    inline bool IsServiceRunning() {
        const std::string name = InstanceMutexNameForGameDir(PluginPaths::InGameDir(""));
        HANDLE mutex = OpenMutexA(SYNCHRONIZE, FALSE, name.c_str());
        if (!mutex) {
            return false;
        }
        CloseHandle(mutex);
        return true;
    }

    inline void EnsureLaunched() {
        static DWORD lastAttemptTick = 0;
        static bool  missingLogged   = false;

        const ApConfig::Settings cfg = ApConfig::LoadFromGameDir();
        if (!cfg.autoConnect) {
            return;
        }
        if (IsServiceRunning()) {
            return;
        }

        const DWORD now = GetTickCount();
        if (lastAttemptTick != 0 && now - lastAttemptTick < 5000) {
            return;
        }
        lastAttemptTick = now;

        const std::string exePath = ResolveExecutablePath();
        if (exePath.empty()) {
            if (!missingLogged) {
                Logger::Log("AP bridge launcher: missing '%s', '%s' and dev fallback '%s'",
                            PluginPaths::InGameDir(kExecutableName).c_str(),
                            PluginPaths::InModuleDir(kExecutableName).c_str(),
                            PluginPaths::InGameDir("..\\gta3-mission-selector\\output\\Release\\GTA3_AP.exe").c_str());
                missingLogged = true;
            }
            return;
        }
        missingLogged = false;

        char commandLine[MAX_PATH * 4] = {};
        std::snprintf(commandLine, sizeof(commandLine),
                      "\"%s\" --parent-pid=%lu \"--game-dir=%s\"",
                      exePath.c_str(), GetCurrentProcessId(), PluginPaths::InGameDir("").c_str());

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::string workingDir = PluginPaths::InGameDir("");
        const BOOL ok = CreateProcessA(
            exePath.c_str(),
            commandLine,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDir.c_str(),
            &si,
            &pi);
        if (!ok) {
            Logger::Log("AP bridge launcher: CreateProcess failed for '%s' (err=%lu)",
                        exePath.c_str(), GetLastError());
            return;
        }

        Logger::Log("AP bridge launcher: started '%s' pid=%lu",
                    exePath.c_str(), pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
