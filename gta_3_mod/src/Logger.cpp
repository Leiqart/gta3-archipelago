#include "Logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <float.h>
#include <mutex>

namespace {
    FILE*       g_file = nullptr;
    std::mutex  g_mutex;

    void WriteTimestamp() {
        std::time_t t = std::time(nullptr);
        std::tm     tm{};
        localtime_s(&tm, &t);
        std::fprintf(g_file, "[%02d:%02d:%02d] ",
                     tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
}

void Logger::Init(const std::string& filename) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) return;
    fopen_s(&g_file, filename.c_str(), "w");
    if (g_file) {
        WriteTimestamp();
        std::fprintf(g_file, "Logger initialised\n");
        std::fflush(g_file);
    }
}

void Logger::Log(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file) return;

    // GTA3 + D3D8 run the x87 FPU in single-precision (_PC_24) mode, especially
    // while the menu/pause screen is rendering. The CRT's %f formatting assumes
    // full precision; under _PC_24 its decimal-conversion loop overflows an
    // internal stack buffer and smashes the *caller's* frame. This surfaced as
    // a 0xC0000005 at the menu-process hook's epilogue (EBX/ESP restored as 0).
    // Force a sane precision for the duration of the format, then restore the
    // game's FPU mode so we don't perturb its own math.
    unsigned int prevControl = 0;
    _controlfp_s(&prevControl, _PC_53, _MCW_PC);

    WriteTimestamp();
    va_list args;
    va_start(args, fmt);
    std::vfprintf(g_file, fmt, args);
    va_end(args);
    std::fputc('\n', g_file);
    std::fflush(g_file);

    unsigned int restored = 0;
    _controlfp_s(&restored, prevControl, _MCW_PC);
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        WriteTimestamp();
        std::fprintf(g_file, "Logger shutdown\n");
        std::fclose(g_file);
        g_file = nullptr;
    }
}
