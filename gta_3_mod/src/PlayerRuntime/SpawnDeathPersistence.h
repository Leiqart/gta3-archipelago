#pragma once
// PlayerRuntime: SpawnDeathPersistence (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    inline void LaunchSpawnCarStub(int model, float x, float y, float z) {
        const std::int16_t ix = EncodeScmFloat(x);
        const std::int16_t iy = EncodeScmFloat(y);
        const std::int16_t iz = EncodeScmFloat(z);
        const auto m = static_cast<std::uint8_t>(model);
        const std::uint8_t stub[] = {
            0x47, 0x02, 0x04, m,                                   // REQUEST_MODEL m
            0x8B, 0x03,                                            // LOAD_ALL_MODELS_NOW
            0xA5, 0x00,                                            // CREATE_CAR
            0x04, m,                                               //   model (INT8)
            0x06, static_cast<std::uint8_t>(ix & 0xFF), static_cast<std::uint8_t>((ix >> 8) & 0xFF), // x
            0x06, static_cast<std::uint8_t>(iy & 0xFF), static_cast<std::uint8_t>((iy >> 8) & 0xFF), // y
            0x06, static_cast<std::uint8_t>(iz & 0xFF), static_cast<std::uint8_t>((iz >> 8) & 0xFF), // z
            0x03, 0x00, 0x00,                                      //   -> local var 0 (handle)
            // SET_CAR_PROOFS localvar0, bullet, fire, explosion, collision, melee
            // (GTA III opcode 0x02AC). All five set -> a fully indestructible
            // debug car: shrugs off gunfire, fire, blasts, crashes and melee.
            0xAC, 0x02,                                            // SET_CAR_PROOFS
            0x03, 0x00, 0x00,                                      //   car = local var 0
            0x04, 0x01,                                            //   bulletproof
            0x04, 0x01,                                            //   fireproof
            0x04, 0x01,                                            //   explosionproof
            0x04, 0x01,                                            //   collisionproof
            0x04, 0x01,                                            //   meleeproof
            0x49, 0x02, 0x04, m,                                   // MARK_MODEL_AS_NO_LONGER_NEEDED m
            0xC3, 0x01, 0x03, 0x00, 0x00,                          // MARK_CAR_AS_NO_LONGER_NEEDED localvar0
            0x4E, 0x00,                                            // TERMINATE_THIS_SCRIPT
        };
        // A script CREATE_CAR makes a permanent "mission car" that never
        // despawns. Without MARK_CAR_AS_NO_LONGER_NEEDED every F6 spawn leaks a
        // vehicle-pool slot, so after enough spawns CREATE_CAR (here or in a
        // mission) returns NULL and the engine crashes positioning the null car
        // (CEntity model lookup at 0x4755C0, read [null+0x5C]). Marking it lets
        // the car despawn normally once the player drives away.
        // The spawn stub's free region is [kSize-160, kSize-96) = 64 bytes
        // (kWantedTrapStubOffset starts at kSize-96).
        CRunningScript* s = LaunchScmStub(kSpawnCarStubOffset,
                                          stub,
                                          ScmSlots::SpawnCarBytes,
                                          "spawn-car");
        if (s) {
            Logger::Log("Spawn-car stub queued: model=%d at (%.1f,%.1f,%.1f) ip=%u",
                        model, x, y, z, kSpawnCarStubOffset);
        }
    }

    // Give the player a full weapon loadout via a bytecode stub: GET_PLAYER_CHAR
    // (&528 = the CREATE_PLAYER handle) then one GIVE_WEAPON_TO_CHAR per weapon.
    // The GTA III opcode handler streams each weapon model in itself, so no
    // REQUEST_MODEL is needed. Weapon ids are GTA III eWeaponType. 9999 ammo each.
    inline void LaunchGiveWeaponsStub() {
        // {type} pairs encoded inline below. INT8 type (0x04), INT16 ammo (0x05).
        constexpr std::uint8_t A_LO = 0x0F, A_HI = 0x27;  // 9999 ammo, little-endian
        auto give = [&](std::uint8_t type) -> std::array<std::uint8_t, 10> {
            return {0xB2, 0x01,             // GIVE_WEAPON_TO_CHAR
                    0x03, 0x00, 0x00,       //   char = local var 0
                    0x04, type,             //   weapon type (INT8)
                    0x05, A_LO, A_HI};      //   ammo = 9999 (INT16)
        };
        std::uint8_t stub[128] = {};
        std::size_t n = 0;
        const std::uint8_t header[] = {
            0xF5, 0x01,                     // GET_PLAYER_CHAR
            0x02, 0x10, 0x02,               //   player = global &528
            0x03, 0x00, 0x00,               //   -> local var 0 (char handle)
        };
        std::memcpy(stub + n, header, sizeof(header));
        n += sizeof(header);
        // bat, colt45, uzi, shotgun, ak47, m16, sniper, rocket, flamethrower,
        // molotov, grenade (GTA III eWeaponType 1..11, skipping 0=unarmed).
        for (std::uint8_t type = 1; type <= 11; ++type) {
            const auto op = give(type);
            std::memcpy(stub + n, op.data(), op.size());
            n += op.size();
        }
        stub[n++] = 0x4E;                   // TERMINATE_THIS_SCRIPT
        stub[n++] = 0x00;

        CRunningScript* s = LaunchScmStub(kGiveWeaponsStubOffset,
                                          stub,
                                          n,
                                          ScmSlots::GiveWeaponsBytes,
                                          "give-weapons");
        if (s) {
            Logger::Log("Give-weapons stub queued: %zu bytes, ip=%u", n, kGiveWeaponsStubOffset);
        }
    }

    // Objective teleport (F1): an exact analogue of the F5 crossing teleport,
    // but reading a PRELOADED in-memory list (g_objectivePositions) instead of
    // the hard-coded kCrossingPoints. The list is filled once, at mission launch
    // (PreloadObjectivePositions), from positions.ini — so by the time F1 is
    // pressed we already have a real list and the keypress does nothing but read
    // it + CPed::Teleport + LoadScene + toast, frame-for-frame like F5.
    inline std::vector<RecordedPos> g_objectivePositions;
    inline int g_objectiveTpIndex = 0;

    // Load one mission's recorded positions into the active F1 list. Called at
    // mission launch so the list is ready before the player can press F1.
    inline void PreloadObjectivePositions(int mission) {
        g_objectivePositions.clear();
        g_objectiveTpIndex = 0;
        if (mission < 0) {
            return;
        }
        LoadRecordedPositions();
        const auto it = g_recordedPositions.find(mission);
        if (it != g_recordedPositions.end()) {
            g_objectivePositions = it->second;
        }
        Logger::Log("Objective positions preloaded for mission %d: %zu point(s)",
                    mission, g_objectivePositions.size());
    }

    // Exact copy of the F5 crossing-teleport handler (TryFireBridgeTeleportKey),
    // the only difference being the source array: the preloaded per-mission list
    // g_objectivePositions instead of the hard-coded kCrossingPoints. No health
    // guard, no mission-fail handling — just like F5. If the list is empty the
    // key does nothing (never warps to a stale/garbage spot).
    inline bool g_objectiveTeleportKeyDown = false;  // TryFireObjectiveTeleportKey -> debug/DebugHotkeys.h

    inline bool g_carKeyDown = false;  // TryFireSpawnCarKey -> debug/DebugHotkeys.h
    inline bool g_weaponsKeyDown = false;  // TryFireGiveWeaponsKey -> debug/DebugHotkeys.h

    // ===== Safehouse teleport (F9): cycle the three safehouses =====
    // Each F9 press warps Claude to the next safehouse so the (now save-free)
    // entry can be checked: Portland, Staunton, Shoreside. Wraps around.
    struct SafehousePoint {
        const SpawnPoint* spawn;
        const char*       toastKey;
    };
    inline const SafehousePoint kSafehousePoints[] = {
        {&kIndustrialSafehouse, "APSH0"}, // Portland
        {&kCommercialSafehouse, "APSH1"}, // Staunton
        {&kSuburbanSafehouse,   "APSH2"}, // Shoreside
    };
    inline int g_safehouseIndex = 0;

    inline bool g_safehouseKeyDown = false;  // TryFireSafehouseTeleportKey -> debug/DebugHotkeys.h

    // ===== Package teleport (F8): hop onto each hidden package in turn =====
    // Cycles through the hidden-package world positions (PackagePoints.h, from
    // ARCHIPELAGO_CHECKS.csv) so each collectable can be reached and tested.
    // Same teleport primitive as the safehouse/crossing keys.
    inline int  g_packageIndex   = 0;
    inline bool g_packageKeyDown = false;  // TryFirePackageTeleportKey -> debug/DebugHotkeys.h

    struct PlayerSkinCandidate {
        const char* name;
    };
    inline constexpr PlayerSkinCandidate kPlayerSkinCandidates[] = {
        {"PLAYERX"},
        {"PLAYER"},
        {"PLAYERP"},
    };
    inline int  g_playerSkinCycleIndex = 0;
    inline bool g_cyclePlayerSkinKeyDown = false;  // TryFireCyclePlayerSkinKey -> debug/DebugHotkeys.h
    inline void SaveLastPlayerSkinTest(const char* skinName) {
        const std::string path = PluginPaths::InGameDir("III.MissionSelector.last_skin.txt");
        FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "w");
        if (!file) {
            Logger::Log("Cycle player skin: failed to write '%s'", path.c_str());
            return;
        }
        std::fprintf(file, "%s\n", skinName ? skinName : "");
        std::fclose(file);
    }

    // ===== Hidden-package milestones: a check every 10 collected =====
    // Reads the engine's collected-package count (CWorld::Players[0],
    // 0x009413A4) each frame; when it crosses a new multiple of 10 (10..100)
    // it reports hidden_packages_<n> as an AP check. AddLocationChecked writes
    // III.Archipelago.state.json, which the bridge turns into a LocationChecks
    // packet. Idempotent: each milestone fires its check at most once.
    inline int g_lastPackageCount = -1;
    inline void TryApplyPackageMilestoneWatch() {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        const int count = *reinterpret_cast<int*>(
            GameAddr::Translate(GameAddr::CWorld_Players0_CollectedPackages));
        if (count < 0 || count > 1000) {
            return;  // player-info not live yet / implausible value
        }
        if (count == g_lastPackageCount) {
            return;
        }
        // A single-step increment is a live pickup: identify WHICH package by
        // proximity (the player stands on it when it registers) and remember
        // its index so a resumed run knows what was already collected. Only
        // meaningful once the restore latch armed — before that the engine
        // count is the fresh-game zero, not progress.
        if (g_playerStateRestoredThisSession &&
            g_lastPackageCount >= 0 && count == g_lastPackageCount + 1) {
            CVector ppos{};
            if (TryReadPlayerPos(&ppos)) {
                constexpr float kMatchRadiusSq = 15.0f * 15.0f;
                int bestIndex = -1;
                float bestDistSq = kMatchRadiusSq;
                constexpr int kPointCount = static_cast<int>(
                    sizeof(kHiddenPackagePoints) / sizeof(kHiddenPackagePoints[0]));
                for (int i = 0; i < kPointCount; ++i) {
                    const float dx = kHiddenPackagePoints[i].x - ppos.x;
                    const float dy = kHiddenPackagePoints[i].y - ppos.y;
                    const float dz = kHiddenPackagePoints[i].z - ppos.z;
                    const float distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        bestIndex = i;
                    }
                }
                if (bestIndex >= 0) {
                    RunState::MarkPackageCollected(bestIndex);
                }
            }
        }
        g_lastPackageCount = count;
        for (int m = 10; m <= 100; m += 10) {
            if (count < m) {
                break;
            }
            char loc[24];
            std::snprintf(loc, sizeof(loc), "hidden_packages_%d", m);
            if (ApState::AddLocationChecked(loc)) {
                char buf[24];
                std::snprintf(buf, sizeof(buf), "PACKAGES %d!", m);
                TextOverrides::SetBridgeToast(buf);
                LaunchPrintNowStub("APBRG", 3000);
                Logger::Log("Package milestone %d reached -> check %s", m, loc);
            }
        }
    }

    // ===== Death hotkey (H): real engine death + nearest-hospital respawn =====
    // Zeroing the player ped's m_fHealth lets CGameLogic::Update run the vanilla
    // wasted sequence, which respawns Claude at the closest hospital — exactly
    // the "real engine death" behaviour we want. The engine strips the player's
    // weapons during that respawn, so when Config::KeepWeaponsOnDeath() is set
    // we snapshot the inventory before the death and re-give it once the engine
    // has restored Claude's health on the far side of the respawn.
    using GiveWeaponFn = int(__thiscall*)(void* ped, int weaponType, int ammo);

    struct WeaponSlotSnapshot {
        int type;
        int ammo;
    };

    enum class DeathPhase {
        Idle,
        AwaitingRespawn,
    };

    inline DeathPhase g_deathPhase      = DeathPhase::Idle;
    inline int        g_deathWatchFrames = 0;
    inline bool       g_deathKeyDown    = false;
    inline WeaponSlotSnapshot g_savedWeapons[GameAddr::kWeaponSlotCount] = {};

    // The wasted fade + reposition takes a few seconds; bail out after this
    // many frames so a missed health rising-edge never wedges the watcher.
    constexpr int kDeathRespawnTimeoutFrames = 60 * 20; // 20 s @ 60 fps

    inline float ReadPedHealth(void* ped) {
        return *reinterpret_cast<float*>(
            static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_fHealth);
    }

    inline void SnapshotPlayerWeapons(void* ped) {
        auto* base = static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_weapons;
        int kept = 0;
        for (int slot = 0; slot < GameAddr::kWeaponSlotCount; ++slot) {
            auto* entry = base + slot * GameAddr::kWeaponEntryStride;
            g_savedWeapons[slot].type =
                *reinterpret_cast<int*>(entry + GameAddr::kWeaponType_Off);
            g_savedWeapons[slot].ammo =
                *reinterpret_cast<int*>(entry + GameAddr::kWeaponAmmoTotal_Off);
            if (g_savedWeapons[slot].type != 0) {
                ++kept;
            }
        }
        Logger::Log("Death: snapshotted %d weapon(s) before respawn", kept);
    }

    inline void RestorePlayerWeapons(void* ped) {
        auto giveWeapon = reinterpret_cast<GiveWeaponFn>(
            GameAddr::Translate(GameAddr::CPed_GiveWeapon));
        int restored = 0;
        for (int slot = 0; slot < GameAddr::kWeaponSlotCount; ++slot) {
            const WeaponSlotSnapshot& w = g_savedWeapons[slot];
            if (w.type == 0) {
                continue;
            }
            __try {
                giveWeapon(ped, w.type, w.ammo);
                ++restored;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("Death: GiveWeapon(type=%d) raised 0x%08lX",
                            w.type, GetExceptionCode());
            }
        }
        Logger::Log("Death: restored %d weapon(s) after respawn", restored);
    }

    inline void TriggerDeath() {
        if (g_deathPhase != DeathPhase::Idle) {
            return;
        }
        void* ped = PlayerPed();
        if (!ped) {
            Logger::Log("Death: no player ped, ignoring hotkey");
            return;
        }

        float health = 0.0f;
        __try {
            health = ReadPedHealth(ped);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Death: reading health raised 0x%08lX", GetExceptionCode());
            return;
        }
        if (health <= 0.0f) {
            Logger::Log("Death: Claude already down (health=%.1f), ignoring", health);
            return;
        }

        const bool keep = Config::KeepWeaponsOnDeath();
        if (keep) {
            __try {
                SnapshotPlayerWeapons(ped);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("Death: weapon snapshot raised 0x%08lX", GetExceptionCode());
            }
        }

        __try {
            *reinterpret_cast<float*>(
                static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_fHealth) = 0.0f;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Death: zeroing health raised 0x%08lX", GetExceptionCode());
            return;
        }

        g_deathPhase = DeathPhase::AwaitingRespawn;
        g_deathWatchFrames = 0;
        Logger::Log("Death triggered: player health zeroed, awaiting hospital respawn (keep_weapons=%d)",
                    keep ? 1 : 0);
    }

    // TryFireDeathKey -> debug/DebugHotkeys.h (calls TriggerDeath, kept here)

    inline void TryApplyDeathRespawnWatch() {
        if (g_deathPhase != DeathPhase::AwaitingRespawn) {
            return;
        }
        ++g_deathWatchFrames;

        void* ped = PlayerPed();
        if (ped) {
            float health = 0.0f;
            bool ok = true;
            __try {
                health = ReadPedHealth(ped);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                ok = false;
            }
            // The engine restores Claude's health (~100) at the tail of the
            // wasted respawn. That rising edge is our cue to re-give the
            // weapons it cleared on the way out.
            if (ok && health > 1.0f) {
                if (Config::KeepWeaponsOnDeath()) {
                    RestorePlayerWeapons(ped);
                }
                Logger::Log("Death: respawn detected (health=%.1f) after %d frames",
                            health, g_deathWatchFrames);
                g_deathPhase = DeathPhase::Idle;
                return;
            }
        }

        if (g_deathWatchFrames >= kDeathRespawnTimeoutFrames) {
            Logger::Log("Death: respawn watch timed out after %d frames; disarming",
                        g_deathWatchFrames);
            g_deathPhase = DeathPhase::Idle;
        }
    }

    // ===== Player loadout/vitals/position/packages persistence ==============
    // Counterpart of TryApplyMoneyPersistence for everything else the engine
    // forgets on a new game: weapon slots, health/armor, world position and the
    // hidden-package counter. Restore once per session from RunState, then
    // mirror the live ped back on a slow tick so quitting at any moment loses
    // at most ~1.5 s of state.

    inline bool TryReadMissionScriptActive() {
        __try {
            return *reinterpret_cast<std::uint8_t*>(
                GameAddr::Translate(GameAddr::CTheScripts_bAlreadyRunningAMissionScript)) != 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline void RestorePlayerStateFromRunState(void* ped) {
        // Weapons: engine-correct re-give, same call the death respawn uses.
        int types[RunState::kPersistWeaponSlots] = {};
        int ammo[RunState::kPersistWeaponSlots] = {};
        if (RunState::SavedWeapons(types, ammo)) {
            auto giveWeapon = reinterpret_cast<GiveWeaponFn>(
                GameAddr::Translate(GameAddr::CPed_GiveWeapon));
            int restored = 0;
            for (int slot = 0; slot < RunState::kPersistWeaponSlots; ++slot) {
                if (types[slot] == 0 || ammo[slot] < 0) {
                    continue;
                }
                __try {
                    giveWeapon(ped, types[slot], ammo[slot]);
                    ++restored;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    Logger::Log("Resume: GiveWeapon(type=%d) raised 0x%08lX",
                                types[slot], GetExceptionCode());
                }
            }
            Logger::Log("Resume: restored %d weapon slot(s)", restored);
        }

        // Vitals: never restore a dead/unsaved state (health <= 0).
        const int health = RunState::SavedHealth();
        if (health > 0) {
            const float fHealth = static_cast<float>((std::min)(health, 250));
            const float fArmor  = static_cast<float>(
                (std::min)((std::max)(RunState::SavedArmor(), 0), 250));
            __try {
                auto* base = static_cast<std::uint8_t*>(ped);
                *reinterpret_cast<float*>(base + GameAddr::CPed_m_fHealth)  = fHealth;
                *reinterpret_cast<float*>(base + GameAddr::CPed_m_fArmour) = fArmor;
                Logger::Log("Resume: vitals restored health=%.0f armor=%.0f",
                            fHealth, fArmor);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("Resume: vitals write raised 0x%08lX", GetExceptionCode());
            }
        }

        // Hidden-package counter: the engine reset it to 0 on new game; put the
        // run's progress back so milestone math continues from where it was.
        const int packages = RunState::CollectedPackages();
        if (packages > 0) {
            __try {
                *reinterpret_cast<int*>(GameAddr::Translate(
                    GameAddr::CWorld_Players0_CollectedPackages)) = packages;
                Logger::Log("Resume: hidden-package count restored to %d", packages);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Logger::Log("Resume: package count write raised 0x%08lX",
                            GetExceptionCode());
            }
        }

        // Position last: ride the vehicle-aware teleport used by the debug
        // warps, then force the scene streamed so Claude does not fall through
        // unloaded map. Skipped when the resume warp already landed on the
        // saved spot — re-teleporting was the "tp bizarre".
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (RunState::SavedPosition(&x, &y, &z)) {
            const CVector pos{x, y, z};
            CVector cur{};
            const bool alreadyThere = TryReadPlayerPos(&cur) &&
                (cur.x - pos.x) * (cur.x - pos.x) +
                (cur.y - pos.y) * (cur.y - pos.y) < 5.0f * 5.0f;
            if (!alreadyThere &&
                TryTeleportVehicleAware(pos, nullptr, "Resume position", true)) {
                TryLoadSceneAt(pos, "Resume position");
                TryLoadAllRequestedModels("Resume position");
            }
        }
    }

    inline void MirrorPlayerStateToRunState(void* ped) {
        // Weapons + vitals straight off the ped, SEH-guarded as one block; a
        // transient bad ped pointer skips the whole mirror tick.
        int types[RunState::kPersistWeaponSlots] = {};
        int ammo[RunState::kPersistWeaponSlots] = {};
        float health = 0.0f;
        float armor = 0.0f;
        CVector pos{};
        bool ok = true;
        __try {
            auto* base = static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_weapons;
            for (int slot = 0; slot < RunState::kPersistWeaponSlots; ++slot) {
                auto* entry = base + slot * GameAddr::kWeaponEntryStride;
                types[slot] = *reinterpret_cast<int*>(entry + GameAddr::kWeaponType_Off);
                ammo[slot]  = *reinterpret_cast<int*>(entry + GameAddr::kWeaponAmmoTotal_Off);
            }
            health = *reinterpret_cast<float*>(
                static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_fHealth);
            armor = *reinterpret_cast<float*>(
                static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_fArmour);
            const float* p = reinterpret_cast<const float*>(
                static_cast<std::uint8_t*>(ped) + kOff_PedPosition);
            pos.x = p[0];
            pos.y = p[1];
            pos.z = p[2];
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok = false;
        }
        if (!ok) {
            return;
        }
        // Never persist the wasted state: the respawn wipes weapons and the
        // death watch re-gives them moments later; a mirror tick in between
        // would write the stripped inventory to disk.
        if (health <= 1.0f || g_deathPhase == DeathPhase::AwaitingRespawn) {
            return;
        }

        RunState::SetSavedWeapons(types, ammo);
        RunState::SetSavedVitals(static_cast<int>(health), static_cast<int>(armor));
        // Skip the position mirror while a mission script runs: resuming inside
        // a half-done mission area without the mission would strand the player.
        // The post-mission safehouse restore makes the next tick save sane.
        if (!TryReadMissionScriptActive()) {
            RunState::SetSavedPosition(pos.x, pos.y, pos.z);
        }

        const int packages = *reinterpret_cast<int*>(GameAddr::Translate(
            GameAddr::CWorld_Players0_CollectedPackages));
        if (packages >= 0 && packages <= 1000) {
            // Engine count only grows during a session; RunState may be ahead
            // briefly before the restore latch fires, never behind.
            if (packages > RunState::CollectedPackages()) {
                RunState::SetCollectedPackages(packages);
            }
        }
    }

    inline void TryApplyPlayerPersistence() {
        if (!RunState::IsRunLive()) {  // offline = no run: don't restore saved position (the "tp") / vitals
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        // Same spawn gates as the money latch: while the boot SCM settles or a
        // warp is pending, the ped state is transient and must not be touched.
        if (g_pendingSafehouseWarp || RunState::IsRunStartPending()) {
            return;
        }
        if (IsMissionRuntimeBusy()) {
            return;
        }
        // Never restore weapons/vitals/position into the intro/GML sequence
        // of a vanilla-style boot — Claude is mid-cinematic or driving 8-Ball.
        if (IsIntroSequencePendingValidation()) {
            return;
        }
        // Same teardown shield as the money mirror: with a menu page open the
        // engine may be unloading the session, and a mirror tick would persist
        // zeroed health/weapons over the real run state.
        if (MenuMgr::CurrentPage() != MenuPage::None) {
            return;
        }
        void* ped = PlayerPed();
        if (!ped) {
            return;
        }

        if (!g_playerStateRestoredThisSession) {
            RestorePlayerStateFromRunState(ped);
            g_playerStateRestoredThisSession = true;
            return;
        }

        if (++g_playerSyncFrames < kPlayerSyncIntervalFrames) {
            return;
        }
        g_playerSyncFrames = 0;
        MirrorPlayerStateToRunState(ped);
    }

}
