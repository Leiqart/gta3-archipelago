#include "PluginPaths.h"

#include <string.h>

namespace {
    std::string g_moduleDir = ".";
    std::string g_gameDir   = ".";

    std::string ParentDir(const std::string& path) {
        const std::size_t slash = path.find_last_of("\\/");
        if (slash == std::string::npos) {
            return {};
        }
        return path.substr(0, slash);
    }

    std::string BaseName(const std::string& path) {
        const std::size_t slash = path.find_last_of("\\/");
        if (slash == std::string::npos) {
            return path;
        }
        return path.substr(slash + 1);
    }

    std::string JoinPath(const std::string& dir, const char* filename) {
        if (!filename || filename[0] == '\0') {
            return dir;
        }
        if (dir.empty() || dir == ".") {
            return filename;
        }
        return dir + "\\" + filename;
    }
}

void PluginPaths::InitFromPath(const char* rawPath) {
    if (!rawPath || rawPath[0] == '\0') {
        g_moduleDir = ".";
        g_gameDir = ".";
        return;
    }
    const std::string path(rawPath);
    g_moduleDir = ParentDir(path);
    g_gameDir = g_moduleDir;

    if (_stricmp(BaseName(g_moduleDir).c_str(), "scripts") == 0) {
        const std::string parent = ParentDir(g_moduleDir);
        if (!parent.empty()) {
            g_gameDir = parent;
        }
    }

    if (g_moduleDir.empty()) {
        g_moduleDir = ".";
    }
    if (g_gameDir.empty()) {
        g_gameDir = g_moduleDir;
    }
}

void PluginPaths::Init(HMODULE module) {
    char modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(module, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        InitFromPath(nullptr);
        return;
    }
    modulePath[length] = '\0';
    InitFromPath(modulePath);
}

void PluginPaths::OverrideGameDir(const char* path) {
    if (path && path[0] != '\0') {
        g_gameDir = path;
    }
}

std::string PluginPaths::InModuleDir(const char* filename) {
    return JoinPath(g_moduleDir, filename);
}

std::string PluginPaths::InGameDir(const char* filename) {
    return JoinPath(g_gameDir, filename);
}
