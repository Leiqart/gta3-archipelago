#pragma once

#include <string>

namespace Logger {
    void Init(const std::string& filename);
    void Log(const char* fmt, ...);
    void Shutdown();
}
