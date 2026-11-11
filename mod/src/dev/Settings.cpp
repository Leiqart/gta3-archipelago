#include <fstream>
#include <string>
#include "../Log.h"
#include "Settings.h"

namespace {
    std::string g_startAt;
    bool g_quitOnStuck = false;
    bool g_quitWhenDone = false;
    bool g_runInBackground = false;
    bool g_mute = false;
    bool g_play = true;
    bool g_skipJobs = false;
    std::string g_skip;
}

void Settings::Load(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = line.substr(0, equals), value = line.substr(equals + 1);
        if (key == "start_at") g_startAt = value;
        else if (key == "quit_on_stuck") g_quitOnStuck = value == "1";
        else if (key == "quit_when_done") g_quitWhenDone = value == "1";
        else if (key == "run_in_background") g_runInBackground = value == "1";
        else if (key == "mute") g_mute = value == "1";
        else if (key == "play") g_play = value != "0";
        else if (key == "skip_jobs") g_skipJobs = value == "1";
        else if (key == "skip") g_skip = "," + value + ",";
    }
    Log::Write("Dev settings: start_at=" + (g_startAt.empty() ? std::string("(first)") : g_startAt) + ", quit_on_stuck=" + std::to_string(g_quitOnStuck) +
               ", quit_when_done=" + std::to_string(g_quitWhenDone) + ", run_in_background=" + std::to_string(g_runInBackground) + ", mute=" + std::to_string(g_mute) + ", play=" + std::to_string(g_play) + ", skip_jobs=" + std::to_string(g_skipJobs) + ", skip=" + (g_skip.empty() ? std::string("(none)") : g_skip.substr(1, g_skip.size() - 2)));
}

const std::string& Settings::StartAt() { return g_startAt; }
bool Settings::QuitOnStuck() { return g_quitOnStuck; }
bool Settings::QuitWhenDone() { return g_quitWhenDone; }
bool Settings::RunInBackground() { return g_runInBackground; }
bool Settings::Mute() { return g_mute; }
bool Settings::Play() { return g_play; }
bool Settings::SkipJobs() { return g_skipJobs; }
bool Settings::Skips(const std::string& script) { return g_skip.find("," + script + ",") != std::string::npos; }
