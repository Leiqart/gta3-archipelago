#include "UI.h"
#include <cstring>

namespace {
    bool Key(const UI::Entry& entry, const char* key) {
        return std::strncmp(entry.key, key, sizeof(entry.key)) == 0;
    }
    void RunLabel(UI::Entry& entry) {
        std::memset(entry.key, 0, sizeof(entry.key));
        std::memcpy(entry.key, "FE_ARCH", 7);
    }
}

bool UI::IsManualStoragePage(int page) {
    switch (page) {
    case 8: case 9: case 11: case 12: case 13: case 14: case 15:
    case 16: case 17: case 19: case 20: case 22: case 23: case 24:
    case 25: case 26: case 27: case 43: case 44: case 45: case 46:
    case 47: case 48: case 49: return true;
    default: return false;
    }
}

void UI::Configure(Screen* screens, int count) {
    if (!screens || count <= 52) return;
    // Preserve vanilla new-game action/confirmation, not the load-slot action.
    Entry run = screens[2].entries[0];
    RunLabel(run);
    for (int page = 0; page < count; ++page) {
        int write = 0;
        for (auto entry : screens[page].entries) {
            if (entry.action == 0 && entry.key[0] == 0) continue;
            const bool storageAction = entry.action == 22 || entry.action == 23 ||
                (entry.action >= 39 && entry.action <= 45) || entry.action == 48;
            if (storageAction || Key(entry, "GMLOAD") || Key(entry, "FES_DGA") ||
                Key(entry, "FES_SGA") || (entry.action == 2 && IsManualStoragePage(entry.target))) continue;
            if (page == 52 && (Key(entry, "FEN_STA") || (entry.action == 2 && entry.target == 2))) continue;
            if (page == 2 && write == 0) RunLabel(entry);
            if (page == 51 && (Key(entry, "FEN_STA") || (entry.action == 2 && entry.target == 2))) entry = run;
            screens[page].entries[write++] = entry;
        }
        while (write < 18) screens[page].entries[write++] = {};
    }
}
