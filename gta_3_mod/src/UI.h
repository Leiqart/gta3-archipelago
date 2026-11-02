#pragma once
#include <cstdint>

namespace UI {
    struct Entry {
        std::int32_t action;
        char key[8];
        std::int32_t saveSlot, target;
    };
    struct Screen {
        char key[8];
        std::int32_t unknown, previous, previousGame, parentEntry, parentGameEntry;
        Entry entries[18];
    };
    static_assert(sizeof(Entry) == 0x14);
    static_assert(sizeof(Screen) == 0x184);

    bool IsManualStoragePage(int page);
    void Configure(Screen* screens, int count);
    // Called only after GameRuntime has checked the executable and initialized MinHook.
    bool Install();
    void Shutdown();
}
