#include "Log.h"
#include <cstdio>
#include <fstream>
#include <mutex>

namespace {
    // Where the log goes, set once by Init.
    std::string g_path;
    // Several threads write to the log; this lock makes them take turns so lines never mix.
    std::mutex g_lock;
}

void Log::Init(const std::string& path) {
    g_path = path;
    std::remove(g_path.c_str());
}

void Log::Write(const std::string& text) {
    std::lock_guard<std::mutex> turn(g_lock);
    std::ofstream log(g_path, std::ios::app);
    log << text << "\n";
}
