#pragma once

#include <string>
#include <windows.h>

namespace PluginPaths {
    void Init(HMODULE module);
    void InitFromPath(const char* path);
    void OverrideGameDir(const char* path);

    std::string InModuleDir(const char* filename);
    std::string InGameDir(const char* filename);
}
