#pragma once

namespace TextOverrides {
    const wchar_t* Lookup(const char* key);

    // Compose a selector label "<marker> <name>" for the given state
    // ('V' validated, 'X' locked, anything else = available) and return a stable
    // (interned) pointer. Used by the CText hook to render marked keys.
    const wchar_t* ComposeMissionMarker(char state, const wchar_t* name);

    // Marked-label registry. MenuPatch registers an entry (its real GXT key +
    // state) and gets back a short synthetic key ("APKnnnn") to use as the menu
    // entry's label. When the engine resolves that key, the CText hook calls
    // ResolveMarked to recover the original key + state, fetches the real name
    // via the original CText, and prefixes the marker. One stable index per
    // distinct original key (re-registering just refreshes its state), so the
    // registry stays bounded. outKey must hold at least 9 chars.
    void RegisterMarked(char* outKey, const char* originalKey, char state);
    bool ResolveMarked(const char* key, const char** outOriginalKey, char* outState);

    // Store the text the external bridge wants shown; the CText hook returns it
    // for the "APBRG" key (used by the bridge-driven PRINT_NOW toast).
    void SetBridgeToast(const char* utf8);
    void RegisterUnlockToast(char* outKey, const char* originalKey);
    bool ResolveUnlockToast(const char* key, const char** outOriginalKey);
    const wchar_t* ComposeUnlockToast(const wchar_t* name);
}
