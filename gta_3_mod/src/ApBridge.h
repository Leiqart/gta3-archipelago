#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "ApState.h"
#include "Logger.h"
#include "PluginPaths.h"

// The external Python bridge writes one-shot
// commands to III.Archipelago.bridge.json:
//
//   { "seq": 3, "toast": "test" }
//
// The plugin polls the file; whenever "seq" changes it executes the command
// (for now: pop "toast" on screen). This is the first proof that the external
// bridge can drive the running game. The sequence number makes each command
// fire exactly once. Later commands (grant item, force sync, …) extend the same
// file + dispatch.
namespace ApBridge {
    constexpr char kBridgeFilename[] = "III.Archipelago.bridge.json";
    constexpr char kStatusFilename[] = "III.Archipelago.status.json";
    // Written by the bridge after it replaced III.MissionSelector.state.ini
    // with the Archipelago data-storage copy: {"seq": N}. The plugin reloads
    // RunState and re-arms its restore latches whenever seq changes.
    constexpr char kSyncFilename[]   = "III.Archipelago.sync.json";

    inline int         g_lastSeq = -999999;
    inline int         g_lastSyncSeq = -999999;
    // "settled" bumps every time the bridge finished its data-storage GET
    // decision for a connect (download applied, local kept, or nothing
    // stored) — the ASI uses it to know the run data is final for this boot.
    inline int         g_lastSettledSeq = -999999;
    inline std::string g_toast;
    // Live connection state the bridge maintains in III.Archipelago.status.json
    // (true while connected/authenticated to an Archipelago server).
    inline bool        g_connected = false;

    inline std::string ReadFile() {
        std::ifstream in(PluginPaths::InGameDir(kBridgeFilename), std::ios::binary);
        if (!in.is_open()) {
            return std::string();
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    inline std::string ReadSyncFile() {
        std::ifstream in(PluginPaths::InGameDir(kSyncFilename), std::ios::binary);
        if (!in.is_open()) {
            return std::string();
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Read the current seq at boot so a stale command left in the file does not
    // fire on the first poll — only genuine changes made while running do.
    // RunState::Init reads whatever state file is on disk at the same moment,
    // so latching the current sync seq here cannot skip a download.
    inline void Init() {
        const std::string s = ReadFile();
        if (!s.empty()) {
            g_lastSeq = ApState::ReadInt(s, "seq", g_lastSeq);
        }
        const std::string sync = ReadSyncFile();
        if (!sync.empty()) {
            g_lastSyncSeq = ApState::ReadInt(sync, "seq", g_lastSyncSeq);
            g_lastSettledSeq = ApState::ReadInt(sync, "settled", g_lastSettledSeq);
        }
        Logger::Log("ApBridge: init (lastSeq=%d, lastSyncSeq=%d, lastSettledSeq=%d)",
                    g_lastSeq, g_lastSyncSeq, g_lastSettledSeq);
    }

    // Returns true exactly once per completed data-storage GET decision.
    inline bool PollStorageSettled() {
        const std::string s = ReadSyncFile();
        if (s.empty()) {
            return false;
        }
        const int seq = ApState::ReadInt(s, "settled", g_lastSettledSeq);
        if (seq == g_lastSettledSeq) {
            return false;
        }
        g_lastSettledSeq = seq;
        return true;
    }

    // Returns true exactly once per data-storage download the bridge applied.
    inline bool PollStateSync() {
        const std::string s = ReadSyncFile();
        if (s.empty()) {
            return false;
        }
        const int seq = ApState::ReadInt(s, "seq", g_lastSyncSeq);
        if (seq == g_lastSyncSeq) {
            return false;
        }
        g_lastSyncSeq = seq;
        return true;
    }

    // Refresh the connection flag from the bridge-owned status file.
    inline void ReadStatus() {
        std::ifstream in(PluginPaths::InGameDir(kStatusFilename), std::ios::binary);
        if (!in.is_open()) {
            g_connected = false;
            return;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        g_connected = ApState::ReadBool(ss.str(), "connected", false);
    }

    // Returns true exactly once per new command; g_toast holds its text.
    inline bool Poll() {
        const std::string s = ReadFile();
        if (s.empty()) {
            return false;
        }
        const int seq = ApState::ReadInt(s, "seq", g_lastSeq);
        if (seq == g_lastSeq) {
            return false;
        }
        g_lastSeq = seq;
        g_toast = ApState::ReadScalarString(s, "toast", "test");
        return true;
    }
}
