#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>
#include <share.h>

namespace {
    FILE* g_log = nullptr;
    std::recursive_mutex g_mutex;

    // Builds the log path next to the ASI file loaded by the game.
    std::string BuildLogPath(HMODULE module) {
        char modulePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(module, modulePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            return "GTA3Archipelago.log";
        }

        std::string path(modulePath, length);
        const std::size_t slash = path.find_last_of("\\/");
        path.resize(slash == std::string::npos ? 0 : slash + 1);
        path += "GTA3Archipelago.log";
        return path;
    }
}

// Opens a new log, replacing the one from any previous session.
void Log::Open(HMODULE module) {
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    Close();
    const std::string path = BuildLogPath(module);
    g_log = _fsopen(path.c_str(), "w", _SH_DENYNO);
}

// Writes a timestamped line and flushes it to disk immediately.
void Log::Write(const char* format, ...) {
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!g_log) {
        return;
    }

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::fprintf(g_log, "[%02u:%02u:%02u] ",
                 static_cast<unsigned int>(time.wHour),
                 static_cast<unsigned int>(time.wMinute),
                 static_cast<unsigned int>(time.wSecond));

    va_list args;
    va_start(args, format);
    std::vfprintf(g_log, format, args);
    va_end(args);

    std::fputc('\n', g_log);
    std::fflush(g_log);
}

// Closes the log file if it is currently open.
void Log::Close() {
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (g_log) {
        std::fclose(g_log);
        g_log = nullptr;
    }
}
