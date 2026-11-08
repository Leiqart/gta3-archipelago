#pragma once
#include <string>

// Our log file next to the .asi: one line per event of this game session.
namespace Log {
    // Remembers where the log goes and starts a fresh file for this game session.
    void Init(const std::string& path);
    // Appends one line to the log; safe to call from any thread.
    void Write(const std::string& text);
}
