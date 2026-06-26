#pragma once
//
// Debug hotkey handlers, separated from PlayerRuntime.h to keep the gameplay
// core readable. These are the F-key / letter-key entry points (teleports,
// spawn car, traps, kill NPCs, position recorder, unlocks, debug validate,
// death). They live in `namespace PlayerRuntime` and are #included at the END
// of PlayerRuntime.h, so every shared primitive they call (PlayerPed,
// LaunchPrintNowStub, TriggerDeath, the teleport primitive, the kXxxPoints data,
// the g_*KeyDown flags, EndMissionInPlace, ...) is already defined above.
//
// Only the hotkey *entry* functions live here; the shared stubs/data/watches
// they rely on stay in PlayerRuntime.h because the core also uses several of
// them (e.g. the death weapon-snapshot is shared with the respawn watch).
//
// Bindings: III.MissionSelector.keys.ini.
//
// The whole module is compiled out in Release (AP_ENABLE_DEBUG_KEYS == 0); the
// #else below provides no-op stubs so DebugManager's per-frame calls still
// resolve.

#include "../BuildConfig.h"   // AP_ENABLE_DEBUG_KEYS

#if AP_ENABLE_DEBUG_KEYS

namespace PlayerRuntime {

    inline void TryFireTestKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("test_print")) & 0x8000) != 0;
        if (down && !g_testKeyDown) {
            LaunchTestPrintStub();
        }
        g_testKeyDown = down;
    }

    inline void TryFireTrapKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("wanted_trap")) & 0x8000) != 0;
        if (down && !g_trapKeyDown) {
            LaunchWantedTrapStub();
        }
        g_trapKeyDown = down;
    }

    inline void TryFireClearWantedKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("clear_wanted")) & 0x8000) != 0;
        if (down && !g_clearWantedKeyDown) {
            LaunchClearWantedStub();
            LaunchPrintNowStub("APNOCOP");
        }
        g_clearWantedKeyDown = down;
    }

    inline void TryFireVehicleSmokeKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("vehicle_smoke")) & 0x8000) != 0;
        if (down && !g_smokeKeyDown) {
            if (SetPlayerVehicleHealth(kVehicleSmokeHealth, "Vehicle smoke trap")) {
                LaunchPrintNowStub("APSMOK");
            }
        }
        g_smokeKeyDown = down;
    }

    inline void TryFireVehicleFireKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("vehicle_fire")) & 0x8000) != 0;
        if (down && !g_fireKeyDown) {
            if (SetPlayerVehicleHealth(kVehicleFireHealth, "Vehicle fire trap")) {
                LaunchPrintNowStub("APFIRE");
            }
        }
        g_fireKeyDown = down;
    }

    // F7 island-unlock trigger RETIRED. Islands now open on their own from
    // EffectiveMapState(): an island opens once a contact that lives on it is
    // unlocked (via that contact's mission OR its AP check) or the AP region
    // item arrives — see PlayerRuntime::ContactUnlockMapState. No manual key,
    // no 3-minute third-island timer. Kept as a no-op so the per-frame dispatch
    // still resolves; F4 (unlock everything) remains for debugging.
    inline void TryFireUnlockIslandKey() {}

    inline void TryFireUnlockCharacterKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("unlock_character")) & 0x8000) != 0;
        if (down && !g_unlockCharacterKeyDown) {
            const int current = RunState::UnlockedCharacters();
            if (current >= ScriptRuntime::kCharacterCount) {
                Logger::Log("Unlock character: all %d characters already unlocked",
                            ScriptRuntime::kCharacterCount);
                LaunchPrintNowStub("APNALL");
            } else {
                RunState::SetUnlockedCharacters(current + 1);
                Logger::Log("Unlock character: %d -> %d", current,
                            RunState::UnlockedCharacters());
                LaunchPrintNowStub("APNCHR");
            }
        }
        g_unlockCharacterKeyDown = down;
    }

    // Debug: unlock EVERYTHING in one press (F4) — all mission contacts and all
    // islands. Hardcoded to F7's sibling F-key so it can't collide with the
    // in-vehicle letter controls. map_state=2 is applied by PushMapStateToScript
    // next frame (drops the bridge barriers); the F7 third-island delay is
    // bypassed on purpose for debugging.
    inline void TryFireUnlockEverythingKey() {
        const bool down = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        if (down && !g_unlockEverythingKeyDown) {
            RunState::SetUnlockedCharacters(ScriptRuntime::kCharacterCount);
            RunState::SetMapState(2);
            Logger::Log("Unlock everything (F4): %d contacts unlocked + map_state=2 (all islands)",
                        ScriptRuntime::kCharacterCount);
            LaunchPrintNowStub("APNALL");
        }
        g_unlockEverythingKeyDown = down;
    }

    inline void TryFireValidateMissionKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("validate_mission")) & 0x8000) != 0;
        if (down && !g_validateMissionKeyDown) {
            const int launched = RunState::LastLaunchedMission();
            const int done = RunState::PendingValidationMission();
            const char* syntheticKey = RunState::PendingValidationSyntheticMission();
            if (launched < 0) {
                Logger::Log("Validate mission: no mission launched this session");
            } else {
                if (syntheticKey && syntheticKey[0] != 0) {
                    if (!RunState::IsSyntheticMissionValidated(syntheticKey)) {
                        RunState::ValidateSyntheticMission(syntheticKey);
                        LaunchPrintNowStub("APVALID");
                        Logger::Log("Validate mission: validated synthetic key='%s' (debug key)",
                                    syntheticKey);
                    } else {
                        Logger::Log("Validate mission: synthetic key='%s' already validated",
                                    syntheticKey);
                    }
                } else if (done >= 0) {
                    if (!RunState::IsMissionValidated(done)) {
                        RunState::ValidateMission(done);
                        LaunchPrintNowStub("APVALID");
                        Logger::Log("Validate mission: validated index=%d (debug key)", done);
                    } else {
                        Logger::Log("Validate mission: index=%d already validated", done);
                    }
                } else {
                    Logger::Log("Validate mission: launch has no pending validation target");
                }
                EndMissionInPlace();
                RunState::ClearPendingValidationMission();
                RunState::SetLastLaunchedMission(-1);
            }
        }
        g_validateMissionKeyDown = down;
    }

    inline void TryFireRecordPositionKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("record_position")) & 0x8000) != 0;
        if (down && !g_recordPosKeyDown) {
            const int mission = RunState::LastLaunchedMission();
            CVector pos{};
            if (mission < 0) {
                LaunchPrintNowStub("APNOMIS");
                Logger::Log("Position recorder: keypress seen but no launched mission "
                            "(onMissionFlag=%d) — nothing saved",
                            ScriptRuntime::IsPlayerOnMission() ? 1 : 0);
            } else {
                float heading = 0.0f;
                // Fall back to the on-foot player position/heading when not in a
                // vehicle, so GML spots reached on foot (e.g. the cutscene exit)
                // can be recorded too — not just driving waypoints.
                if (!TryReadVehiclePos(&pos) || !TryReadVehicleHeading(&heading)) {
                    if (!TryReadPlayerPos(&pos) || !TryReadPlayerHeading(&heading)) {
                        Logger::Log("Position recorder: no player pos/heading, ignoring");
                        g_recordPosKeyDown = down;
                        return;
                    }
                }
                if (!g_recordedLoaded) {
                    LoadRecordedPositions();
                }
                g_recordedPositions[mission].push_back({pos.x, pos.y, pos.z, heading});
                SaveRecordedPositions();
                const int count = static_cast<int>(g_recordedPositions[mission].size());
                char recKey[9] = {};
                std::snprintf(recKey, sizeof(recKey), "APR%03d", count > 999 ? 999 : count);
                LaunchPrintNowStub(recKey);
                Logger::Log("Recorded vehicle position #%d for mission %d: (%.2f,%.2f,%.2f) h=%.1f",
                            count, mission, pos.x, pos.y, pos.z, heading);
            }
        }
        g_recordPosKeyDown = down;
    }

    inline void TryFireBridgeTeleportKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("crossing_teleport")) & 0x8000) != 0;
        if (down && !g_bridgeKeyDown) {
            const CrossingPoint& cp = kCrossingPoints[g_crossingIndex];
            float vehicleHeading = 0.0f;
            const float* headingPtr = TryReadVehicleHeading(&vehicleHeading)
                ? &vehicleHeading
                : nullptr;
            if (TryTeleportVehicleAware(cp.pos, headingPtr, "Crossing teleport", true)) {
                TryLoadSceneAt(cp.pos, "Crossing teleport");
                TryLoadAllRequestedModels("Crossing teleport");
                if (ScriptRuntime::HasLiveScriptEngine()) {
                    LaunchPrintNowStub(cp.toastKey);
                }
                g_crossingIndex = (g_crossingIndex + 1) %
                    static_cast<int>(sizeof(kCrossingPoints) / sizeof(kCrossingPoints[0]));
            }
        }
        g_bridgeKeyDown = down;
    }

    inline void TryFireObjectiveTeleportKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("objective_teleport")) & 0x8000) != 0;
        if (down && !g_objectiveTeleportKeyDown) {
            // The boot-intro path never goes through LaunchMission, so the
            // per-mission list is never preloaded during GML. Fill it on demand
            // from the active mission index instead of hard-blocking: the GML
            // waypoints live in positions.ini ([mission 2]/[mission 21]) and the
            // user wants K to cycle them during GML like any other mission.
            if (g_objectivePositions.empty() &&
                (IsIntroSequencePendingValidation() || IsIntroSequenceMissionActive())) {
                PreloadObjectivePositions(RunState::LastLaunchedMission());
            }
            if (g_objectivePositions.empty()) {
                Logger::Log("Objective teleport: no positions for this mission, ignoring");
                LaunchPrintNowStub("APNOMIS");
            } else {
                if (g_objectiveTpIndex >= static_cast<int>(g_objectivePositions.size())) {
                    g_objectiveTpIndex = 0;
                }
                const RecordedPos& p = g_objectivePositions[g_objectiveTpIndex];
                const CVector pos{p.x, p.y, p.z};
                if (TryTeleportVehicleAware(pos, &p.heading, "Objective teleport", true)) {
                    TryLoadSceneAt(pos, "Objective teleport");
                    TryLoadAllRequestedModels("Objective teleport");
                    if (ScriptRuntime::HasLiveScriptEngine()) {
                        char tpKey[9] = {};
                        std::snprintf(tpKey, sizeof(tpKey), "APO%03d", g_objectiveTpIndex + 1);
                        LaunchPrintNowStub(tpKey);
                    }
                    g_objectiveTpIndex = (g_objectiveTpIndex + 1) %
                        static_cast<int>(g_objectivePositions.size());
                }
            }
        }
        g_objectiveTeleportKeyDown = down;
    }

    inline void TryFireSpawnCarKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("spawn_car")) & 0x8000) != 0;
        if (down && !g_carKeyDown) {
            if (!ScriptRuntime::HasLiveScriptEngine()) {
                Logger::Log("Spawn car: no live script engine, ignoring");
            } else {
                CVector pos{};
                if (!TryReadPlayerPos(&pos)) {
                    Logger::Log("Spawn car: no player position, ignoring");
                } else {
                    const int model = kSpawnCarModels[g_spawnCarIndex];
                    LaunchSpawnCarStub(model, pos.x + 3.0f, pos.y, pos.z + 1.0f);
                    LaunchPrintNowStub(model == 101 ? "APCAR" : "APCAR2");
                    g_spawnCarIndex = (g_spawnCarIndex + 1) %
                        static_cast<int>(sizeof(kSpawnCarModels) / sizeof(kSpawnCarModels[0]));
                }
            }
        }
        g_carKeyDown = down;
    }

    inline void TryFireGiveWeaponsKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("give_weapons")) & 0x8000) != 0;
        if (down && !g_weaponsKeyDown) {
            if (!ScriptRuntime::HasLiveScriptEngine()) {
                Logger::Log("Give weapons: no live script engine, ignoring");
            } else {
                LaunchGiveWeaponsStub();
                TextOverrides::SetBridgeToast("WEAPONS!");
                LaunchPrintNowStub("APBRG", 2500);
            }
        }
        g_weaponsKeyDown = down;
    }

    inline void TryFireSafehouseTeleportKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("safehouse_teleport")) & 0x8000) != 0;
        if (down && !g_safehouseKeyDown) {
            const SafehousePoint& sh = kSafehousePoints[g_safehouseIndex];
            const CVector pos = sh.spawn->position;
            if (TryTeleportVehicleAware(pos, &sh.spawn->heading, "Safehouse teleport", true)) {
                TryLoadSceneAt(pos, "Safehouse teleport");
                TryLoadAllRequestedModels("Safehouse teleport");
                if (ScriptRuntime::HasLiveScriptEngine()) {
                    LaunchPrintNowStub(sh.toastKey);
                }
                g_safehouseIndex = (g_safehouseIndex + 1) %
                    static_cast<int>(sizeof(kSafehousePoints) / sizeof(kSafehousePoints[0]));
            }
        }
        g_safehouseKeyDown = down;
    }

    inline void TryFirePackageTeleportKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("package_teleport")) & 0x8000) != 0;
        if (down && !g_packageKeyDown) {
            constexpr int kCount =
                static_cast<int>(sizeof(kHiddenPackagePoints) / sizeof(kHiddenPackagePoints[0]));
            const PackagePoint& pp = kHiddenPackagePoints[g_packageIndex];
            CVector pos; pos.x = pp.x; pos.y = pp.y; pos.z = pp.z;
            float vehicleHeading = 0.0f;
            const float* headingPtr = TryReadVehicleHeading(&vehicleHeading)
                ? &vehicleHeading
                : nullptr;
            if (TryTeleportVehicleAware(pos, headingPtr, "Package teleport", true)) {
                TryLoadSceneAt(pos, "Package teleport");
                TryLoadAllRequestedModels("Package teleport");
                if (ScriptRuntime::HasLiveScriptEngine()) {
                    char buf[24];
                    std::snprintf(buf, sizeof(buf), "PACKAGE %d/%d",
                                  g_packageIndex + 1, kCount);
                    TextOverrides::SetBridgeToast(buf);
                    LaunchPrintNowStub("APBRG", 2500);
                }
                g_packageIndex = (g_packageIndex + 1) % kCount;
            }
        }
        g_packageKeyDown = down;
    }

    inline void TryFireCyclePlayerSkinKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("cycle_player_skin")) & 0x8000) != 0;
        if (down && !g_cyclePlayerSkinKeyDown) {
            if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
                Logger::Log("Cycle player skin: script engine/player not live, ignoring");
            } else {
                const PlayerSkinCandidate& skin = kPlayerSkinCandidates[g_playerSkinCycleIndex];
                if (LaunchApplyPlayerSkinStub(skin.name, false, "manual cycle key")) {
                    g_bootOutfitRestoreDone = true; // do not auto-restore over the manual test
                    g_bootOutfitSettleFrames = 0;

                    char toast[64] = {};
                    std::snprintf(toast, sizeof(toast), "SKIN: %s", skin.name);
                    TextOverrides::SetBridgeToast(toast);
                    LaunchPrintNowStub("APBRG", 3000);
                    Logger::Log("Cycle player skin: applied '%s' (%d/%d)",
                                skin.name,
                                g_playerSkinCycleIndex + 1,
                                static_cast<int>(sizeof(kPlayerSkinCandidates) /
                                                 sizeof(kPlayerSkinCandidates[0])));
                    SaveLastPlayerSkinTest(skin.name);
                    g_playerSkinCycleIndex =
                        (g_playerSkinCycleIndex + 1) %
                        static_cast<int>(sizeof(kPlayerSkinCandidates) /
                                         sizeof(kPlayerSkinCandidates[0]));
                }
            }
        }
        g_cyclePlayerSkinKeyDown = down;
    }

    inline void TryFireDeathKey() {
        const bool down = (GetAsyncKeyState(DebugKeys::Key("death")) & 0x8000) != 0;
        if (down && !g_deathKeyDown) {
            TriggerDeath();
        }
        g_deathKeyDown = down;
    }

}  // namespace PlayerRuntime

#else  // AP_ENABLE_DEBUG_KEYS == 0 : cheats compiled OUT (Release distribution).

// No-op stubs so the per-frame TryFire*Key() calls in DebugManager still resolve
// and no cheat/debug code is pulled into the binary. Covers every debug hotkey
// entry point the manager polls (including TryFireKillNpcsKey, whose real impl
// lives in PlayerRuntime/StubsValidation.h and is #if'd out there too).
namespace PlayerRuntime {
    inline void TryFireTestKey() {}
    inline void TryFireDeathKey() {}
    inline void TryFireTrapKey() {}
    inline void TryFireClearWantedKey() {}
    inline void TryFireUnlockIslandKey() {}
    inline void TryFireUnlockCharacterKey() {}
    inline void TryFireUnlockEverythingKey() {}
    inline void TryFireValidateMissionKey() {}
    inline void TryFireObjectiveTeleportKey() {}
    inline void TryFireKillNpcsKey() {}
    inline void TryFireRecordPositionKey() {}
    inline void TryFireBridgeTeleportKey() {}
    inline void TryFireSpawnCarKey() {}
    inline void TryFireGiveWeaponsKey() {}
    inline void TryFireVehicleSmokeKey() {}
    inline void TryFireVehicleFireKey() {}
    inline void TryFireSafehouseTeleportKey() {}
    inline void TryFirePackageTeleportKey() {}
    inline void TryFireCyclePlayerSkinKey() {}
}  // namespace PlayerRuntime

#endif  // AP_ENABLE_DEBUG_KEYS
