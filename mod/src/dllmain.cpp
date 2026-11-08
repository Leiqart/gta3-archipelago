#include <windows.h>
#include <string>
#include "MinHook.h"
#include "ClientLink.h"
#include "Log.h"
#include "Menu.h"
#include "Missions.h"
#ifdef GTA3AP_DEV
#include "dev/Cheats.h"
#endif

namespace {
    // Our own thread, outside DllMain: starts each module in order, then keeps the menu in sync with the connection.
    DWORD WINAPI ModThread(void*) {
        // MinHook is shared by every module that hooks the game, so it is started once, here.
        if (MH_Initialize() != MH_OK) {
            Log::Write("MinHook failed to start");
            return 1;
        }
        if (!Menu::Install()) return 1;
        ClientLink::Start();
        for (;;) {
            Menu::Update();
            Sleep(50);
        }
    }
}

// Windows calls DllMain when the game loads or unloads our ASI.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // The log goes next to our own .asi file, whatever folder the game was started from.
        char path[MAX_PATH] = {};
        GetModuleFileNameA(module, path, MAX_PATH);
        std::string folder = path;
        folder = folder.substr(0, folder.find_last_of('\\') + 1);
        Log::Init(folder + "GTA3Archipelago.log");
        Log::Write("Hello from GTA3Archipelago!");
        // Here and not in ModThread: hooking the game loop must happen before the game runs it; Missions first, so its events are ready for the others.
        Missions::Install();
#ifdef GTA3AP_DEV
        Cheats::Install(folder);
#endif
        CreateThread(nullptr, 0, ModThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
