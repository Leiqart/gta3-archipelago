#include "UI.h"
#include "Log.h"
#include "../third_party/minhook/include/MinHook.h"
#include <windows.h>
#include <array>
#include <cstring>

namespace {
    constexpr auto kProcess = 0x485100;
    constexpr auto kText = 0x52C5A0;
    constexpr auto kScreens = 0x611930;
    using Process = void(__thiscall*)(void*);
    using Text = const wchar_t*(__thiscall*)(void*, const char*);
    Process originalProcess = nullptr;
    Text originalText = nullptr;
    std::array<UI::Screen, 59> backup{};
    bool installed = false;

    const wchar_t* __fastcall GetText(void* self, void*, const char* key) {
        static const wchar_t label[] = L"Run Archipelago";
        if (key && std::strcmp(key, "FE_ARCH") == 0) return label;
        return originalText(self, key);
    }

    void __fastcall ProcessMenu(void* self, void*) {
        auto* manager = static_cast<unsigned char*>(self);
        auto& page = *reinterpret_cast<int*>(manager + 0x548);
        auto& selection = *reinterpret_cast<int*>(manager + 0x54C);
        if (UI::IsManualStoragePage(page)) {
            // Also catch safehouse-triggered menus, which do not use a menu row.
            const bool saveZone = manager[0x453] != 0;
            manager[0x454] = 0; // no vanilla load request
            if (saveZone) {
                manager[0x453] = 0;
                *reinterpret_cast<bool*>(0x95CD6A) = true; // RequestFrontEndShutDown
                page = 52;
            } else {
                page = manager[0x116] ? 51 : 52;
            }
            selection = 0;
        }
        originalProcess(self);
    }

    bool WriteScreens(const UI::Screen* screens) {
        DWORD protection;
        auto* target = reinterpret_cast<void*>(kScreens);
        if (!VirtualProtect(target, sizeof(backup), PAGE_READWRITE, &protection)) return false;
        std::memcpy(target, screens, sizeof(backup));
        DWORD ignored;
        VirtualProtect(target, sizeof(backup), protection, &ignored);
        return true;
    }
}

bool UI::Install() {
    if (installed) return true;
    std::memcpy(backup.data(), reinterpret_cast<void*>(kScreens), sizeof(backup));
    auto patched = backup;
    Configure(patched.data(), static_cast<int>(patched.size()));
    if (MH_CreateHook(reinterpret_cast<void*>(kProcess), &ProcessMenu,
            reinterpret_cast<void**>(&originalProcess)) != MH_OK) return false;
    if (MH_CreateHook(reinterpret_cast<void*>(kText), &GetText,
            reinterpret_cast<void**>(&originalText)) != MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(kProcess));
        return false;
    }
    if (!WriteScreens(patched.data()) ||
        MH_EnableHook(reinterpret_cast<void*>(kText)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(kProcess)) != MH_OK) {
        MH_DisableHook(reinterpret_cast<void*>(kProcess));
        MH_DisableHook(reinterpret_cast<void*>(kText));
        MH_RemoveHook(reinterpret_cast<void*>(kProcess));
        MH_RemoveHook(reinterpret_cast<void*>(kText));
        WriteScreens(backup.data());
        return false;
    }
    installed = true;
    Log::Write("Archipelago UI installed; manual save/load/delete disabled");
    return true;
}

void UI::Shutdown() {
    if (!installed) return;
    MH_DisableHook(reinterpret_cast<void*>(kProcess));
    MH_DisableHook(reinterpret_cast<void*>(kText));
    MH_RemoveHook(reinterpret_cast<void*>(kProcess));
    MH_RemoveHook(reinterpret_cast<void*>(kText));
    WriteScreens(backup.data());
    installed = false;
}
