#include "TextOverrides.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#include "ApBridge.h"
#include "GameAddresses.h"
#include "ScriptRuntime.h"

namespace {
    // Marked-label registry (see TextOverrides.h). One stable index per distinct
    // original key; the slot's state is refreshed on every re-registration.
    struct MarkedLabel {
        std::string original;
        char        state;
    };
    std::vector<MarkedLabel>     g_marked;
    std::map<std::string, int>   g_markedIndexByKey;
    std::vector<std::string>     g_unlockToasts;
    std::map<std::string, int>   g_unlockToastIndexByKey;

    // The engine caches the wchar_t* that CText::Get returns. The real
    // CText::Get hands back pointers into the loaded GXT block, which never
    // moves, so callers store them freely: the on-screen PRINT_NOW toast holds
    // its pointer for ~3 s redrawing every frame, and the pause-menu Brief page
    // keeps it indefinitely. Our overrides must offer the same lifetime
    // guarantee. A recycling ring buffer cannot: the menu calls CText::Get
    // ~767x per frame, so a 64-slot ring is reused ~12x per frame and any
    // cached pointer soon shows whatever overwrote its slot (typically the
    // menu's "RUN MISSION"). That is the brief-page / toast corruption we hit.
    //
    // So every distinct override string is interned in a std::map keyed by its
    // own text. std::map nodes never relocate, so c_str() stays valid for the
    // life of the process, and identical strings collapse onto one node. Text
    // lookup runs entirely on the main (render/script) thread, so the static
    // pool needs no synchronisation.
    const wchar_t* Intern(const wchar_t* text) {
        static std::map<std::wstring, std::wstring> pool;
        return pool.emplace(text, text).first->second.c_str();
    }

    // Text last pushed by the external bridge (returned for the "APBRG" key).
    const wchar_t* g_bridgeToast = L"test";

    const wchar_t* FormatText(const wchar_t* fmt, int a) {
        wchar_t buffer[64];
        _snwprintf_s(buffer, _TRUNCATE, fmt, a);
        return Intern(buffer);
    }

    const wchar_t* FormatText(const wchar_t* fmt, int a, int b) {
        wchar_t buffer[64];
        _snwprintf_s(buffer, _TRUNCATE, fmt, a, b);
        return Intern(buffer);
    }

    bool ParseIndexedKey(const char* key, const char* prefix, int* outIndex) {
        if (std::strncmp(key, prefix, 3) != 0) {
            return false;
        }
        if (key[3] < '0' || key[3] > '9' ||
            key[4] < '0' || key[4] > '9' ||
            key[5] < '0' || key[5] > '9') {
            return false;
        }
        *outIndex = (key[3] - '0') * 100 + (key[4] - '0') * 10 + (key[5] - '0');
        return true;
    }

    bool HasCurrentMissionContext() {
        const int actualMissionIndex = RunState::LastLaunchedMission();
        return actualMissionIndex >= 0 &&
               (ScriptRuntime::AlreadyRunningAMissionScript() ||
                ScriptRuntime::IsPlayerOnMission());
    }
}

const wchar_t* TextOverrides::Lookup(const char* key) {
    if (!key || key[0] == '\0') {
        return nullptr;
    }

    // Constant overrides: string literals already have stable, whole-process
    // storage, so return them directly — no interning needed.
    if (std::strncmp(key, "FE_ARCH", 8) == 0) {
        return L"RUN ARCHIPELAGO";
    }
    if (std::strncmp(key, "FE_RUNM", 8) == 0) {
        return HasCurrentMissionContext() ? L"CURRENT MISSION"
                                          : L"RUN MISSION";
    }
    if (std::strncmp(key, "APTITLE", 8) == 0) {
        return HasCurrentMissionContext() ? L"CURRENT MISSION"
                                          : L"RUN MISSION";
    }
    if (std::strncmp(key, "APINTRO", 8) == 0) {
        return L"INTRO";
    }
    if (std::strncmp(key, "APLOAD", 8) == 0) {
        return L"RESUME RUN";
    }
    if (std::strncmp(key, "APBUSY", 8) == 0) {
        return L"FINISH CURRENT MISSION";
    }
    if (std::strncmp(key, "APREDO", 8) == 0) {
        return L"REDO MISSION";
    }
    if (std::strncmp(key, "APQUIT", 8) == 0) {
        return L"QUIT MISSION";
    }
    // Bridge-driven toast: the text last set by ApBridge via SetBridgeToast.
    if (std::strncmp(key, "APBRG", 8) == 0) {
        return g_bridgeToast;
    }
    // Live Archipelago connection indicator (shown in the start menu).
    if (std::strncmp(key, "APCONN", 8) == 0) {
        return ApBridge::g_connected ? L"ARCHIPELAGO: CONNECTED"
                                     : L"ARCHIPELAGO: NOT CONNECTED";
    }
    if (std::strncmp(key, "APCASH", 8) == 0) {
        int cash = 0;
        return GameAddr::TryReadPlayerCash(&cash)
            ? FormatText(L"CASH: $%d", cash)
            : L"CASH: N/A";
    }
    // Test notification key — referenced by the P-key bytecode stub.
    // Triggers a PRINT_NOW 3-second on-screen toast AND, because the flag
    // argument is 1, gets added to the engine's brief log so it shows up
    // in the pause-menu Brief screen too. Both callers cache the pointer,
    // which is why this must be a stable literal rather than a ring buffer.
    if (std::strncmp(key, "TEST_AP", 8) == 0) {
        return L"test";
    }

    // Island-unlock toasts fired by the F7 hotkey (PlayerRuntime).
    if (std::strncmp(key, "APUNL1", 8) == 0) {
        return L"STAUNTON ISLAND UNLOCKED";
    }
    if (std::strncmp(key, "APUNL2", 8) == 0) {
        return L"SHORESIDE ISLAND UNLOCKED";
    }
    if (std::strncmp(key, "APUNLM", 8) == 0) {
        return L"ALL ISLANDS UNLOCKED";
    }
    // Debug hotkey toasts: F6 car spawn (alternates Infernus/Sentinel), F5 bridge teleport.
    if (std::strncmp(key, "APNOCOP", 8) == 0) {
        return L"POLICE CLEARED";
    }
    if (std::strncmp(key, "APCAR2", 8) == 0) {
        return L"SENTINEL SPAWNED";
    }
    if (std::strncmp(key, "APCAR", 8) == 0) {
        return L"INFERNUS SPAWNED";
    }
    // F5 crossing-teleport cycle labels (PlayerRuntime kCrossingPoints).
    if (std::strncmp(key, "APTP0", 8) == 0) {
        return L"CALLAHAN BRIDGE";
    }
    if (std::strncmp(key, "APTP1", 8) == 0) {
        return L"PORTER TUNNEL";
    }
    if (std::strncmp(key, "APTP2", 8) == 0) {
        return L"PORTLAND SUBWAY";
    }
    if (std::strncmp(key, "APTP3", 8) == 0) {
        return L"SHORESIDE TUNNEL";
    }
    if (std::strncmp(key, "APTP4", 8) == 0) {
        return L"SHORESIDE SUBWAY";
    }
    if (std::strncmp(key, "APTP5", 8) == 0) {
        return L"LIFT BRIDGE";
    }
    // Vehicle damage traps (F11/F12).
    if (std::strncmp(key, "APSMOK", 8) == 0) {
        return L"ENGINE SMOKING";
    }
    if (std::strncmp(key, "APFIRE", 8) == 0) {
        return L"CAR ON FIRE";
    }
    // F9 safehouse-teleport cycle labels.
    if (std::strncmp(key, "APSH0", 8) == 0) {
        return L"PORTLAND SAFEHOUSE";
    }
    if (std::strncmp(key, "APSH1", 8) == 0) {
        return L"STAUNTON SAFEHOUSE";
    }
    if (std::strncmp(key, "APSH2", 8) == 0) {
        return L"SHORESIDE SAFEHOUSE";
    }
    // Mission validated toast (completion watch / validate_mission debug key).
    if (std::strncmp(key, "APVALID", 8) == 0) {
        return L"MISSION VALIDATED";
    }
    // Objective-teleport debug key toast.
    if (std::strncmp(key, "APOBJTP", 8) == 0) {
        return L"TELEPORT TO OBJECTIVE";
    }
    // Position-recorder debug key toasts.
    if (std::strncmp(key, "APREC", 8) == 0) {
        return L"POSITION SAVED";
    }
    if (std::strncmp(key, "APNOMIS", 8) == 0) {
        return L"NOT IN A MISSION";
    }
    if (std::strncmp(key, "APNOPOS", 8) == 0) {
        return L"NO SAVED POSITION";
    }
    // Kill-NPCs debug key toast.
    if (std::strncmp(key, "APKILL", 8) == 0) {
        return L"NPCS CLEARED";
    }

    // F2 progressive character unlock.
    if (std::strncmp(key, "APNCHR", 8) == 0) {
        return L"CHARACTER UNLOCKED";
    }
    if (std::strncmp(key, "APNALL", 8) == 0) {
        return L"ALL CHARACTERS UNLOCKED";
    }

    int index = 0;
    // APW### — Shoreside still time-gated, ### seconds remaining.
    if (ParseIndexedKey(key, "APW", &index)) {
        return FormatText(L"SHORESIDE LOCKED - %d s", index);
    }
    // APR### — position recorder toast showing the running count for this
    // mission, so each ² press visibly increments.
    if (ParseIndexedKey(key, "APR", &index)) {
        return FormatText(L"POSITION SAVED #%d", index);
    }
    // APO### — objective-teleport toast: warped to recorded point #.
    if (ParseIndexedKey(key, "APO", &index)) {
        return FormatText(L"WARP TO POINT #%d", index);
    }
    if (ParseIndexedKey(key, "APM", &index)) {
        return FormatText(L"CHK %03d", index + 1);
    }
    if (ParseIndexedKey(key, "APL", &index)) {
        return FormatText(L"LOCKED CHK %03d", index + 1);
    }
    if (ParseIndexedKey(key, "APG", &index) || ParseIndexedKey(key, "APX", &index)) {
        const int missionCount = std::max(0, ScriptRuntime::MissionCount());
        const int start = index * 10 + 1;
        const int end = missionCount > 0
            ? std::min(index * 10 + 10, missionCount)
            : (index * 10 + 10);
        if (std::strncmp(key, "APX", 3) == 0) {
            return FormatText(L"LOCKED %03d-%03d", start, end);
        }
        return FormatText(L"CHECKS %03d-%03d", start, end);
    }

    return nullptr;
}

const wchar_t* TextOverrides::ComposeMissionMarker(char state, const wchar_t* name) {
    if (!name) {
        name = L"";
    }
    // The GTA III font has no glyphs for check/cross/box and remaps square
    // brackets, but renders parentheses correctly (confirmed in-game), so the
    // marker letter is wrapped in parens. V validated, X locked, O available
    // (accessible, not yet validated).
    // The GTA III font has no asterisk glyph, so a bugged mission ('*') is
    // marked with the letter B (Bugged) — letters and parens both render.
    const wchar_t* prefix = state == 'V' ? L"(V) "
                          : state == 'X' ? L"(X) "
                          : state == '*' ? L"(B) "
                                         : L"(O) ";
    wchar_t buffer[96];
    _snwprintf_s(buffer, _TRUNCATE, L"%s%s", prefix, name);
    return Intern(buffer);
}

const wchar_t* TextOverrides::ComposeUnlockToast(const wchar_t* name) {
    if (!name) {
        name = L"";
    }
    wchar_t buffer[128];
    _snwprintf_s(buffer, _TRUNCATE, L"UNLOCKED: %s", name);
    return Intern(buffer);
}

void TextOverrides::SetBridgeToast(const char* utf8) {
    if (!utf8) {
        return;
    }
    // ASCII-widen (enough for the test; extend to MultiByteToWideChar later for
    // accented bridge text). The result is interned so the engine can cache it.
    wchar_t buffer[128];
    int i = 0;
    for (; utf8[i] != '\0' && i < 127; ++i) {
        buffer[i] = static_cast<wchar_t>(static_cast<unsigned char>(utf8[i]));
    }
    buffer[i] = L'\0';
    g_bridgeToast = Intern(buffer);
}

void TextOverrides::RegisterMarked(char* outKey, const char* originalKey, char state) {
    const auto it = g_markedIndexByKey.find(originalKey);
    int index;
    if (it == g_markedIndexByKey.end()) {
        index = static_cast<int>(g_marked.size());
        g_markedIndexByKey.emplace(originalKey, index);
        g_marked.push_back(MarkedLabel{originalKey, state});
    } else {
        index = it->second;
        g_marked[static_cast<std::size_t>(index)].state = state;
    }
    std::snprintf(outKey, 9, "APK%04d", index);
}

bool TextOverrides::ResolveMarked(const char* key, const char** outOriginalKey, char* outState) {
    if (!key || key[0] != 'A' || key[1] != 'P' || key[2] != 'K') {
        return false;
    }
    for (int i = 3; i < 7; ++i) {
        if (key[i] < '0' || key[i] > '9') {
            return false;
        }
    }
    const int index = (key[3] - '0') * 1000 + (key[4] - '0') * 100 +
                      (key[5] - '0') * 10 + (key[6] - '0');
    if (index < 0 || index >= static_cast<int>(g_marked.size())) {
        return false;
    }
    *outOriginalKey = g_marked[static_cast<std::size_t>(index)].original.c_str();
    *outState = g_marked[static_cast<std::size_t>(index)].state;
    return true;
}

void TextOverrides::RegisterUnlockToast(char* outKey, const char* originalKey) {
    const auto it = g_unlockToastIndexByKey.find(originalKey);
    int index;
    if (it == g_unlockToastIndexByKey.end()) {
        index = static_cast<int>(g_unlockToasts.size());
        g_unlockToastIndexByKey.emplace(originalKey, index);
        g_unlockToasts.push_back(originalKey);
    } else {
        index = it->second;
    }
    std::snprintf(outKey, 9, "APU%04d", index);
}

bool TextOverrides::ResolveUnlockToast(const char* key, const char** outOriginalKey) {
    if (!key || key[0] != 'A' || key[1] != 'P' || key[2] != 'U') {
        return false;
    }
    for (int i = 3; i < 7; ++i) {
        if (key[i] < '0' || key[i] > '9') {
            return false;
        }
    }
    const int index = (key[3] - '0') * 1000 + (key[4] - '0') * 100 +
                      (key[5] - '0') * 10 + (key[6] - '0');
    if (index < 0 || index >= static_cast<int>(g_unlockToasts.size())) {
        return false;
    }
    *outOriginalKey = g_unlockToasts[static_cast<std::size_t>(index)].c_str();
    return true;
}
