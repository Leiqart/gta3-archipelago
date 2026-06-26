#pragma once
//
// Build-time feature switches for the MissionSelector ASI.
//
// AP_ENABLE_DEBUG_KEYS — the cheat / debug hotkeys (F2/F4/F7 unlocks, kill NPCs,
// spawn car, give weapons, teleports, position recorder, instant death, ...).
//   Debug build   -> 1 (cheats compiled IN, for testing)
//   Release build -> 0 (cheats compiled OUT, clean distribution binary)
// premake defines _DEBUG only for the Debug configuration. Override by defining
// AP_ENABLE_DEBUG_KEYS on the compiler command line / in premake if you ever want
// a Release build WITH cheats (or a Debug build without).
//
#ifndef AP_ENABLE_DEBUG_KEYS
  #ifdef _DEBUG
    #define AP_ENABLE_DEBUG_KEYS 1
  #else
    #define AP_ENABLE_DEBUG_KEYS 0
  #endif
#endif

//
// Default runtime config (Config.cpp / III.MissionSelector.ini).
//
// These are the values the ASI uses when III.MissionSelector.ini is absent, and
// the values it writes back when it generates a default ini. Centralised here so
// the distributed binary needs NO shipped ini file: deleting the ini just makes
// the ASI re-create it from these defaults. They encode the Archipelago play
// profile (classic street markers, atomic mission unlocks), so override on the
// compiler command line only if you want a different out-of-the-box behaviour.
//
#ifndef AP_DEFAULT_DEBUG_OPEN_ISLANDS
  #define AP_DEFAULT_DEBUG_OPEN_ISLANDS 0
#endif
#ifndef AP_DEFAULT_UNLOCK_MISSIONS_ATOMICALLY
  #define AP_DEFAULT_UNLOCK_MISSIONS_ATOMICALLY 1
#endif
#ifndef AP_DEFAULT_USE_CLASSIC_MISSION_MARKERS
  #define AP_DEFAULT_USE_CLASSIC_MISSION_MARKERS 1
#endif
#ifndef AP_DEFAULT_REQUIRE_AP_MISSION_UNLOCK_ITEMS
  #define AP_DEFAULT_REQUIRE_AP_MISSION_UNLOCK_ITEMS 0
#endif
#ifndef AP_DEFAULT_KEEP_WEAPONS_ON_DEATH
  #define AP_DEFAULT_KEEP_WEAPONS_ON_DEATH 1
#endif
