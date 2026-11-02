#pragma once

#include <windows.h>

namespace Log {
    // Opens a new log next to the ASI file loaded by the game.
    void Open(HMODULE module);

    // Appends a timestamped line to the log if it is open.
    void Write(const char* format, ...);

    // Closes the log and releases its file.
    void Close();
}
