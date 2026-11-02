#pragma once
#include <windows.h>

namespace GameRuntime {
    bool Install(HMODULE module);
    void Shutdown();
    // Only called from hooks on the game thread.
    void Tick();
}
