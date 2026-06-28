#pragma once
// PlayerRuntime: BridgeOutfit (carved from the former monolith, same namespace,
// included in original order by PlayerRuntime.h so visibility is unchanged).

namespace PlayerRuntime {
    inline const ApWeaponItem* FindApWeaponItem(const std::string& itemName) {
        for (const ApWeaponItem& entry : kApWeaponItems) {
            if (itemName == entry.name) {
                return &entry;
            }
        }
        return nullptr;
    }

    inline bool TryGiveWeaponDirect(int weaponType, int ammo) {
        void* ped = PlayerPed();
        if (!ped) {
            return false;
        }
        using GiveFn = int(__thiscall*)(void* ped, int weaponType, int ammo);
        auto giveWeapon = reinterpret_cast<GiveFn>(
            GameAddr::Translate(GameAddr::CPed_GiveWeapon));
        __try {
            giveWeapon(ped, weaponType, ammo);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Item apply: GiveWeapon(type=%d) raised 0x%08lX",
                        weaponType, GetExceptionCode());
            return false;
        }
    }

    inline bool TrySetArmorDirect(int amount) {
        void* ped = PlayerPed();
        if (!ped) {
            return false;
        }
        __try {
            *reinterpret_cast<float*>(
                static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_fArmour) =
                static_cast<float>(amount);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("Item apply: armor write raised 0x%08lX", GetExceptionCode());
            return false;
        }
    }

    // Poll the external bridge/service for one-shot commands (Bloc D). Checked a
    // few times per second (not every frame) to keep the file I/O cheap. Before
    // polling we also auto-launch the native bridge companion if AP autoconnect
    // is enabled and no instance is already alive for this game directory.
    inline int g_bridgePollFrames = 0;
    constexpr int kCashItemValue = 2000;
    inline void TryPollBridge() {
        if (++g_bridgePollFrames < 20) {
            return;
        }
        g_bridgePollFrames = 0;
        ApBridgeProcess::EnsureLaunched();
        ApBridge::ReadStatus();  // refresh the connection indicator
        RunState::SetServerConnected(ApBridge::g_connected);  // gate the live run on it
        const bool stateChanged = ApState::RefreshState();
        // Live-apply items the bridge granted (written to III.Archipelago.items.json),
        // and pop an on-screen toast for each newly received item.
        const bool itemsChanged = ApState::RefreshItems();
        const bool bridgeCommand = ApBridge::Poll();
        if (ApBridge::g_connected && (stateChanged || bridgeCommand)) {
            ApState::TrustCheckedStateThisSession();
        }
        // Connected => recover the full Archipelago inventory; disconnected =>
        // start with nothing. Trust received items as soon as the bridge proves
        // it is live this session (item/state change or a bridge toast such as
        // "AP connected"), not only when items.json content differs. Otherwise a
        // reconnect to the same seed with an unchanged inventory would never arm
        // the latch and the player would appear empty-handed while connected. A
        // stale status.json left "connected" by a crashed session produces none
        // of these signals, so the offline "start with nothing" path holds.
        if (ApBridge::g_connected && (itemsChanged || stateChanged || bridgeCommand)) {
            ApState::TrustReceivedItemsThisSession();
        }
        // Data-storage download: the bridge replaced III.MissionSelector.state.ini
        // with the server copy. Reload it and re-arm both restore latches so the
        // server's money/weapons/vitals/position land on the live ped.
        if (ApBridge::PollStateSync()) {
            RunState::Refresh();
            ResetMoneyPersistenceSession();
            ResetPlayerPersistenceSession();
            TextOverrides::SetBridgeToast("AP STATE SYNCED");
            LaunchPrintNowStub("APBRG", 3000);
            Logger::Log("RunState reloaded from Archipelago data storage (syncSeq=%d)",
                        ApBridge::g_lastSyncSeq);
        }
        if (ApBridge::PollStorageSettled()) {
            g_storageSettledThisBoot = true;
            Logger::Log("Archipelago data storage settled for this boot");
        }
        {
            const auto& items = ApState::ReceivedItems();
            const int itemCount = static_cast<int>(items.size());
            int appliedCount = ApState::ReceivedItemsAppliedCount();
            if (appliedCount > itemCount) {
                ApState::SetReceivedItemsAppliedCount(itemCount);
                appliedCount = itemCount;
            }
            // Items mutate the live player (cash stub, weapon grants), so hold
            // them until Claude is actually spawned and settled; the applied
            // counter only advances once they really landed.
            // g_moneyRestoredThisSession: the session's money restore MUST land
            // before any cash item does, or the restore overwrites the freshly
            // granted +2000 one frame later (seen when GML validation opened
            // both gates in the same frame).
            const bool readyToApply =
                ScriptRuntime::HasLiveScriptEngine() && PlayerPed() &&
                !g_pendingSafehouseWarp && !RunState::IsRunStartPending() &&
                !IsIntroSequencePendingValidation() &&
                MenuMgr::CurrentPage() == MenuPage::None &&
                g_moneyRestoredThisSession;
            if (itemCount > appliedCount && readyToApply) {
                std::string lastItem;
                int predictedCash = GameAddr::ReadPlayerCashOrZero();
                bool cashChanged = false;
                for (int i = appliedCount; i < itemCount; ++i) {
                    const std::string& item = items[static_cast<std::size_t>(i)];
                    lastItem = item;
                    if (item == "cash" || item == "money" || item == "$2000") {
                        LaunchAddScoreStub(kCashItemValue);
                        predictedCash += kCashItemValue;
                        cashChanged = true;
                        // No toast for cash (user request): the Archipelago
                        // client already announces the find with its source
                        // mission; the HUD money counter shows the +2000.
                        lastItem.clear();
                        Logger::Log("ApState: cash item applied (+%d -> %d)",
                                    kCashItemValue, predictedCash);
                    } else if (const ApWeaponItem* weapon = FindApWeaponItem(item)) {
                        if (TryGiveWeaponDirect(weapon->weaponType, weapon->ammo)) {
                            Logger::Log("ApState: weapon item applied '%s' (type=%d ammo=%d)",
                                        item.c_str(), weapon->weaponType, weapon->ammo);
                        }
                    } else if (item == "armor") {
                        if (TrySetArmorDirect(100)) {
                            Logger::Log("ApState: armor item applied (100)");
                        }
                    } else {
                        Logger::Log("ApState: new item state '%s'", item.c_str());
                    }
                }
                ApState::SetReceivedItemsAppliedCount(itemCount);
                if (cashChanged) {
                    RunState::SetMoney(predictedCash);
                    g_lastSyncedMoney = predictedCash;
                    g_moneySyncFrames = 0;
                    Logger::Log("ApState: cash item persisted immediately ($%d)",
                                predictedCash);
                }
                if (!lastItem.empty()) {
                    if (lastItem.rfind("CASH ", 0) == 0) {
                        TextOverrides::SetBridgeToast(lastItem.c_str());
                    } else {
                        TextOverrides::SetBridgeToast(("ITEM: " + lastItem).c_str());
                    }
                    LaunchPrintNowStub("APBRG", 4000);
                }
            }
        }
        if (bridgeCommand) {
            TextOverrides::SetBridgeToast(ApBridge::g_toast.c_str());
            LaunchPrintNowStub("APBRG", 5000);
            Logger::Log("Bridge command: toast '%s' (seq=%d)",
                        ApBridge::g_toast.c_str(), ApBridge::g_lastSeq);
        }
    }

    // ===== Crossing teleport (F5): cycle the inter-island choke points =====
    // Each F5 press warps Claude to the next blockage between islands so they can
    // all be checked quickly: Callahan bridge, Porter tunnel and Portland subway
    // (Portland<->Staunton), then the Staunton<->Shoreside tunnel and subway. The
    // list wraps around, and a toast names each spot.
    struct CrossingPoint {
        CVector     pos;
        const char* toastKey;
    };
    inline const CrossingPoint kCrossingPoints[] = {
        {{811.9f,   -939.95f,  35.8f},  "APTP0"}, // Callahan bridge (Portland<->Staunton road)
        {{730.331f,  172.467f, -21.0f}, "APTP1"}, // Porter tunnel   (Portland<->Staunton tunnel)
        {{988.963f, -471.778f,  5.2f},  "APTP2"}, // Portland subway gate
        {{-58.0f,   -631.0f,   42.0f},  "APTP5"}, // Staunton<->Shoreside lift bridge (the raising span)
        {{533.3f,    97.0f,    -21.3f}, "APTP3"}, // Shoreside tunnel (Staunton<->Shoreside)
        {{-672.0f,  -760.0f,    8.3f},  "APTP4"}, // Shoreside subway gate
    };
    inline int g_crossingIndex = 0;

    inline bool g_bridgeKeyDown = false;  // TryFireBridgeTeleportKey -> debug/DebugHotkeys.h

    // F6 spawns a car a few metres from Claude via a CREATE_CAR bytecode stub.
    // The stub requests + synchronously loads the model, creates the car, then
    // releases the model ref. Float args use GTA III's fixed-point encoding
    // (type 0x06, int16 = value*16); the model is an INT8 (0x04), so it must
    // stay < 128.
    // Alternates each press: INFERNUS (101, fast 2-seat supercar) then SENTINEL
    // (95, fast 4-door so mission passengers/escorts fit). Both < 128.
    constexpr int kSpawnCarModels[] = {101, 95}; // INFERNUS, SENTINEL
    inline int g_spawnCarIndex = 0;
    // Sits at kSize-160 (NOT the -128 packed 32-byte slot): the stub now also
    // does MARK_CAR_AS_NO_LONGER_NEEDED, pushing it past 32 bytes. The 64-byte
    // gap [kSize-160, kSize-96) is free — kVehicleTeleportStubOffset writes a
    // full 96 bytes ending at kSize-160, and kWantedTrapStubOffset starts at
    // kSize-96 — so the spawn stub fits without clobbering either neighbour.
    constexpr std::uint32_t kSpawnCarStubOffset =
        ScmSlots::SpawnCarOffset;

    inline std::int16_t EncodeScmFloat(float f) {
        long v = std::lround(f * 16.0f);
        if (v > 32767) { v = 32767; }
        if (v < -32768) { v = -32768; }
        return static_cast<std::int16_t>(v);
    }

    // Vehicle-first teleport stub for the debug warp keys and the recorded
    // mission positions. It resolves the player's current vehicle handle from
    // the SCM player global, then moves that car in place so every occupant
    // (Claude, mission passengers, escorts) rides along.
    constexpr std::uint32_t kVehicleTeleportStubOffset =
        ScmSlots::VehicleTeleportOffset;
    constexpr std::uint32_t kMissionVehiclePrepStubOffset =
        ScmSlots::MissionVehiclePrepOffset;
    constexpr std::uint32_t kAddScoreStubOffset =
        ScmSlots::AddScoreOffset;
    constexpr std::uint32_t kPrepareIntroModeStubOffset =
        ScmSlots::PrepareIntroModeOffset;
    constexpr std::uint32_t kLoadCollisionStubOffset =
        ScmSlots::LoadCollisionOffset;
    constexpr std::uint32_t kClearContactBlipsStubOffset =
        ScmSlots::ClearContactBlipsOffset;
    constexpr std::uint32_t kRestorePlayerOutfitStubOffset =
        ScmSlots::RestorePlayerOutfitOffset;
    // Debug "give weapons" loadout stub. 128-byte slot [kSize-896, kSize-768),
    // just below the lowest existing stub; still deep in the reliably-zero
    // padding tail of the main-thread region.
    constexpr std::uint32_t kGiveWeaponsStubOffset =
        ScmSlots::GiveWeaponsOffset;
    constexpr std::uint32_t kSetCurrentWeaponStubOffset =
        ScmSlots::SetCurrentWeaponOffset;
    constexpr std::uint16_t kScriptControlledPlayerGlobalOffset = 44;

    inline void AppendScmLocalVar0(std::uint8_t* stub, std::size_t* n) {
        stub[(*n)++] = 0x03;
        stub[(*n)++] = 0x00;
        stub[(*n)++] = 0x00;
    }

    inline void AppendScmPlayerHandle(std::uint8_t* stub, std::size_t* n) {
        stub[(*n)++] = 0x02;
        stub[(*n)++] = 0x10;
        stub[(*n)++] = 0x02;
    }

    inline void AppendScmGlobalVar(std::uint8_t* stub, std::size_t* n,
                                   std::uint16_t offset) {
        stub[(*n)++] = 0x02;
        stub[(*n)++] = static_cast<std::uint8_t>(offset & 0xFF);
        stub[(*n)++] = static_cast<std::uint8_t>((offset >> 8) & 0xFF);
    }

    inline void AppendScmInt16(std::uint8_t* stub, std::size_t* n, std::int16_t value) {
        stub[(*n)++] = 0x05;
        stub[(*n)++] = static_cast<std::uint8_t>(value & 0xFF);
        stub[(*n)++] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    }

    inline void AppendScmFloat(std::uint8_t* stub, std::size_t* n, float f) {
        const std::int16_t v = EncodeScmFloat(f);
        stub[(*n)++] = 0x06;
        stub[(*n)++] = static_cast<std::uint8_t>(v & 0xFF);
        stub[(*n)++] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    }

    inline bool WritePlayingIntro(bool enabled, const char* reason) {
        __try {
            *reinterpret_cast<bool*>(
                GameAddr::Translate(GameAddr::CGame_playingIntro)) = enabled;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("%s: writing CGame::playingIntro raised 0x%08lX",
                        reason, GetExceptionCode());
            return false;
        }
        Logger::Log("%s: CGame::playingIntro=%d", reason, enabled ? 1 : 0);
        return true;
    }

    inline bool LaunchSetCurrentPlayerWeaponStub(int weaponType,
                                                 const char* reason) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return false;
        }
        std::uint8_t stub[64] = {};
        std::size_t n = 0;
        // NOTE: do NOT emit REMOVE_ALL_PLAYER_WEAPONS here. That opcode walks the
        // player ped's RW clump to detach weapon atomics; run during the
        // intro->GML transition (corrupt/half-loaded weapon-model state, or a
        // mid-flight UNDRESS_CHAR redress) it dereferences a freed pointer and
        // crashes (0x5A234E, called from CPed::RemoveWeaponModel @0x4CF983).
        // SET_CURRENT_PLAYER_WEAPON(0) below already unequips Claude (empty
        // hands); fully clearing the inventory must happen from a known-safe SCM
        // point, not this per-frame stub.

        stub[n++] = 0xB8; stub[n++] = 0x01; // SET_CURRENT_PLAYER_WEAPON
        AppendScmPlayerHandle(stub, &n);
        stub[n++] = 0x04;
        stub[n++] = static_cast<std::uint8_t>(weaponType & 0xFF);

        stub[n++] = 0xF5; stub[n++] = 0x01; // GET_PLAYER_CHAR
        AppendScmPlayerHandle(stub, &n);
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);

        stub[n++] = 0xB9; stub[n++] = 0x01; // SET_CURRENT_CHAR_WEAPON
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);
        stub[n++] = 0x04;
        stub[n++] = static_cast<std::uint8_t>(weaponType & 0xFF);

        stub[n++] = 0x4E; stub[n++] = 0x00; // TERMINATE_THIS_SCRIPT

        CRunningScript* s = LaunchScmStub(kSetCurrentWeaponStubOffset,
                                          stub,
                                          n,
                                          ScmSlots::SetCurrentWeaponBytes,
                                          reason ? reason : "weapon fix");
        if (!s) {
            Logger::Log("%s: SET_CURRENT_PLAYER_WEAPON(%d) queue failed",
                        reason ? reason : "weapon fix", weaponType);
            return false;
        }
        Logger::Log("%s: queued SET_CURRENT_PLAYER/CHAR_WEAPON(%d)",
                    reason ? reason : "weapon fix",
                    weaponType);
        return true;
    }

    inline bool IsIntroSequenceMissionActive() {
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        return ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(syntheticKey);
    }

    inline bool IsIntroSequenceLaunchPending() {
        return g_manualIntroTriggerFrames > 0 ||
               g_introBankSceneFramesLeft > 0 ||
               g_introTeleportArmed ||
               g_introGmlSplitGuardFrames > 0 ||
               g_introFollowupLaunchFrames > 0 ||
               g_giveMeLibertyRetryFrames > 0 ||
               Hooks::g_allowNextEngineIntroLaunch ||
               Hooks::g_allowNextEngineEightballLaunch;
    }

    inline bool IsIntroSequencePresentationActive() {
        return IsIntroSequenceMissionActive() || IsIntroSequenceLaunchPending();
    }

    constexpr int kGiveMeLibertyUnarmedFixFrames = 5 * 60 * 60;
    constexpr int kGiveMeLibertyUnarmedDelayFrames = 0;
    constexpr int kGiveMeLibertyUnarmedStubIntervalFrames = 15;
    constexpr int kGiveMeLibertyUnarmedMaxStubAttempts = 120;
    inline int g_giveMeLibertyUnarmedFixFrames = 0;
    inline int g_giveMeLibertyUnarmedStubAttempts = 0;
    inline bool g_giveMeLibertyUnarmedLogged = false;
    inline void ArmGiveMeLibertyUnarmedFix(const char* reason) {
        g_giveMeLibertyUnarmedFixFrames = kGiveMeLibertyUnarmedFixFrames;
        g_giveMeLibertyUnarmedStubAttempts = 0;
        g_giveMeLibertyUnarmedLogged = false;
        Logger::Log("GML: unarmed fix armed for %d frames (%s)",
                    kGiveMeLibertyUnarmedFixFrames,
                    reason ? reason : "unspecified");
    }

    inline void TryApplyGiveMeLibertyUnarmedFix() {
        if (!IsIntroSequencePresentationActive()) {
            g_giveMeLibertyUnarmedFixFrames = 0;
            g_giveMeLibertyUnarmedStubAttempts = 0;
            g_giveMeLibertyUnarmedLogged = false;
            return;
        }
        if (g_giveMeLibertyUnarmedFixFrames <= 0) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return;
        }
        void* ped = PlayerPed();
        if (!ped) {
            return;
        }
        const int elapsed =
            kGiveMeLibertyUnarmedFixFrames - g_giveMeLibertyUnarmedFixFrames;
        bool weaponVisibleOrEquipped = false;
        __try {
            auto* currentWeapon = reinterpret_cast<std::uint8_t*>(
                static_cast<std::uint8_t*>(ped) + GameAddr::CPed_m_currentWeapon);
            if (elapsed >= kGiveMeLibertyUnarmedDelayFrames && *currentWeapon != 0) {
                weaponVisibleOrEquipped = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (!g_giveMeLibertyUnarmedLogged) {
                g_giveMeLibertyUnarmedLogged = true;
                Logger::Log("GML: unarmed weapon read raised 0x%08lX",
                            GetExceptionCode());
            }
        }
        const bool shouldQueueScmWeaponFix =
            elapsed >= kGiveMeLibertyUnarmedDelayFrames &&
            g_giveMeLibertyUnarmedStubAttempts <
                kGiveMeLibertyUnarmedMaxStubAttempts &&
            (weaponVisibleOrEquipped || g_giveMeLibertyUnarmedStubAttempts == 0 ||
             (elapsed % kGiveMeLibertyUnarmedStubIntervalFrames) == 0);
        if (shouldQueueScmWeaponFix) {
            const bool queued =
                LaunchSetCurrentPlayerWeaponStub(0, "GML unarmed weapon fix");
            weaponVisibleOrEquipped = queued || weaponVisibleOrEquipped;
            ++g_giveMeLibertyUnarmedStubAttempts;
        }
        if (weaponVisibleOrEquipped) {
            if (!g_giveMeLibertyUnarmedLogged) {
                g_giveMeLibertyUnarmedLogged = true;
                Logger::Log("GML: queued engine weapon switch to unarmed");
            }
        }
        --g_giveMeLibertyUnarmedFixFrames;
    }

    inline bool HasReachedEightballHideout() {
        __try {
            return *reinterpret_cast<std::int32_t*>(
                       ScriptRuntime::ScriptSpace() + kEightReachedHideoutGlobalOffset) != 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool HasLuigisGirlsStarted() {
        __try {
            return *reinterpret_cast<std::int32_t*>(
                       ScriptRuntime::ScriptSpace() + kLuigisGirlsStartedGlobalOffset) != 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline void TryApplyGiveMeLibertyToLuigisGirlsCleanup() {
        if (g_gmlToLm1CleanupApplied) {
            return;
        }
        if (RunState::LastLaunchedMission() != 21) {
            return;
        }
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        if (!ScriptRuntime::IsIntroOrGiveMeLibertySyntheticKey(syntheticKey)) {
            return;
        }
        const bool reachedHideout = HasReachedEightballHideout();
        const bool luigisGirlsStarted = HasLuigisGirlsStarted();
        if (!reachedHideout && !luigisGirlsStarted) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
            return;
        }

        g_gmlToLm1CleanupApplied = true;
        LaunchApplyPlayerSkinStub("PLAYERX", true, "GML -> LM1 cleanup", true);
        LaunchSetCurrentPlayerWeaponStub(0, "GML -> LM1 cleanup weapon clear");
        ArmGiveMeLibertyUnarmedFix("GML -> LM1 cleanup");
        Logger::Log("GML -> LM1 cleanup applied: reached_hideout=%d luigis_started=%d, skin PLAYERX + unarmed enforced",
                    reachedHideout ? 1 : 0,
                    luigisGirlsStarted ? 1 : 0);
    }

    inline bool IsGiveMeLibertyPhaseActive() {
        if (RunState::LastLaunchedMission() != 21) {
            return false;
        }
        if (HasReachedEightballHideout() || HasLuigisGirlsStarted()) {
            return false;
        }
        const char* syntheticKey = RunState::PendingValidationSyntheticMission();
        if (ScriptRuntime::IsGiveMeLibertySyntheticKey(syntheticKey)) {
            return true;
        }
        if (!ScriptRuntime::IsIntroSequenceSyntheticKey(syntheticKey)) {
            return false;
        }
        return !HasReachedEightballHideout() && !HasLuigisGirlsStarted();
    }

    inline bool LaunchApplyPlayerSkinStub(const char* skinName,
                                          bool clearWanted,
                                          const char* reason,
                                          bool clearWeapons) {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            return false;
        }
        if (!skinName || skinName[0] == '\0') {
            return false;
        }

        std::uint8_t stub[64] = {};
        std::size_t n = 0;

        stub[n++] = 0xF5; stub[n++] = 0x01; // GET_PLAYER_CHAR
        AppendScmPlayerHandle(stub, &n);
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);

        stub[n++] = 0x52; stub[n++] = 0x03; // UNDRESS_CHAR
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);
        // The SCM skin label is an 8-byte fixed string used by UNDRESS_CHAR.
        // "//Player change" redress uses it). Casing is irrelevant — the engine
        // lowercases the label internally (re3 COMMAND_UNDRESS_CHAR: tolower) —
        // so the prison skin sticking was a timing/path miss, not the name.
        std::memcpy(stub + n,
                    skinName,
                    std::min<std::size_t>(std::strlen(skinName), 8));
        n += 8;

        stub[n++] = 0x8B; stub[n++] = 0x03; // LOAD_ALL_MODELS_NOW

        stub[n++] = 0x53; stub[n++] = 0x03; // DRESS_CHAR
        AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);

        if (clearWeapons) {
            stub[n++] = 0xB8; stub[n++] = 0x03; // REMOVE_ALL_PLAYER_WEAPONS
            AppendScmPlayerHandle(stub, &n);

            stub[n++] = 0xB8; stub[n++] = 0x01; // SET_CURRENT_PLAYER_WEAPON
            AppendScmPlayerHandle(stub, &n);
            stub[n++] = 0x04;
            stub[n++] = 0x00; // WEAPONTYPE_UNARMED

            stub[n++] = 0xB9; stub[n++] = 0x01; // SET_CURRENT_CHAR_WEAPON
            AppendScmGlobalVar(stub, &n, kScriptControlledPlayerGlobalOffset);
            stub[n++] = 0x04;
            stub[n++] = 0x00; // WEAPONTYPE_UNARMED
        }

        if (clearWanted) {
            stub[n++] = 0x10; stub[n++] = 0x01; // CLEAR_WANTED_LEVEL
            AppendScmPlayerHandle(stub, &n);
        }

        stub[n++] = 0x4E; stub[n++] = 0x00; // TERMINATE_THIS_SCRIPT

        CRunningScript* thread = LaunchScmStub(kRestorePlayerOutfitStubOffset,
                                               stub,
                                               n,
                                               ScmSlots::RestorePlayerOutfitBytes,
                                               "apply-player-skin");
        if (thread) {
            Logger::Log("Apply-player-skin stub queued: skin='%.8s' clear_weapons=%d (%s)",
                        skinName,
                        clearWeapons ? 1 : 0,
                        reason ? reason : "unspecified");
        }
        return thread != nullptr;
    }

    inline void LaunchRestorePlayerOutfitStub() {
        LaunchApplyPlayerSkinStub("PLAYERX", true, "restore normal outfit");
    }
    // O selects the visual state; cutscene creation queues the exact same
    // UNDRESS/DRESS operation that the working manual key uses. Applying it a
    // couple of frames later avoids launching an SCM helper re-entrantly from
    // CCutsceneMgr's model lookup.
    inline bool g_secondaryPlayerSkinActive = false;
    inline int  g_cutsceneSkinApplyDelayFrames = -1;
    inline int  g_cutsceneSkinApplyWaitFrames = 0;
    inline int  g_cutsceneSkinApplyMission = -1;
    constexpr int kCutsceneSkinApplyDelayFrames = 2;
    constexpr int kCutsceneSkinApplyTimeoutFrames = 3 * 60;

    inline const char* SelectedPlayerSkinName() {
        return g_secondaryPlayerSkinActive ? "PLAYER" : "PLAYERX";
    }

    inline void QueueCutscenePlayerSkinApply(const char* requestedModel) {
        const int mission = RunState::LastLaunchedMission();
        if (mission <= 21 || IsIntroSequencePresentationActive() ||
            IsGiveMeLibertyPhaseActive()) {
            return;
        }
        if (g_cutsceneSkinApplyDelayFrames >= 0 &&
            g_cutsceneSkinApplyMission == mission) {
            return;
        }
        g_cutsceneSkinApplyDelayFrames = kCutsceneSkinApplyDelayFrames;
        g_cutsceneSkinApplyWaitFrames = 0;
        g_cutsceneSkinApplyMission = mission;
        Logger::Log("Cutscene skin apply queued: model='%s' mission=%d selected=%s",
                    requestedModel ? requestedModel : "",
                    mission,
                    SelectedPlayerSkinName());
    }

    inline void TryApplySelectedCutscenePlayerSkin() {
        if (g_cutsceneSkinApplyDelayFrames < 0) {
            return;
        }
        if (++g_cutsceneSkinApplyWaitFrames > kCutsceneSkinApplyTimeoutFrames) {
            Logger::Log("Cutscene skin apply timed out: mission=%d",
                        g_cutsceneSkinApplyMission);
            g_cutsceneSkinApplyDelayFrames = -1;
            g_cutsceneSkinApplyMission = -1;
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
            return;
        }
        if (RunState::LastLaunchedMission() != g_cutsceneSkinApplyMission ||
            !ScriptRuntime::AlreadyRunningAMissionScript() ||
            IsIntroSequencePresentationActive() ||
            IsGiveMeLibertyPhaseActive()) {
            g_cutsceneSkinApplyDelayFrames = -1;
            g_cutsceneSkinApplyMission = -1;
            return;
        }
        if (g_cutsceneSkinApplyDelayFrames > 0) {
            --g_cutsceneSkinApplyDelayFrames;
            return;
        }
        if (PlayerVehicle()) {
            return;
        }
        const char* skinName = SelectedPlayerSkinName();
        if (!LaunchApplyPlayerSkinStub(skinName, false,
                                       "automatic selected cutscene skin")) {
            return;
        }
        Logger::Log("Cutscene skin applied: mission=%d selected=%s",
                    g_cutsceneSkinApplyMission,
                    skinName);
        g_cutsceneSkinApplyDelayFrames = -1;
        g_cutsceneSkinApplyMission = -1;
    }

    // Apply a queued outfit restore once the wasted respawn finished (the ped
    // is alive again); running the stub mid-fade would dress a dead ped.
    inline void TryApplyPendingOutfitRestore() {
        if (!g_outfitRestorePending) {
            return;
        }
        if (IsIntroSequencePresentationActive()) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine() || !PlayerPed()) {
            return;
        }
        if (!SafeReadPlayerAlive() || PlayerVehicle()) {
            return;
        }
        g_outfitRestorePending = false;
        LaunchRestorePlayerOutfitStub();
        Logger::Log("Pending outfit restore applied after respawn");
    }

    // Boot-path safety net for the skin. The SCM "Player change" redress only
    // runs when 8ball.sc executes to its split exit; the synthetic-GML and
    // resume boot paths skip the script entirely, so Claude keeps the brown
    // prison texture forever. Once per engine-life, when a run is active and the
    // player is alive and on foot in free-roam (not mid-mission, not in the
    // intro), force the normal PLAYERX texture back. UNDRESS/DRESS rebuilds the
    // ped clump; doing that while Claude is seated corrupts his vehicle-exit
    // animation/control state and can leave him frozen after getting out.
    inline bool g_bootOutfitRestoreDone = false;
    inline int  g_bootOutfitSettleFrames = 0;
    inline void TryApplyBootOutfitRestore() {
        if (!RunState::IsRunActive()) {
            return;
        }
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            g_bootOutfitRestoreDone = false; // fresh engine: redress again
            g_bootOutfitSettleFrames = 0;
            return;
        }
        if (!PlayerPed() || MenuMgr::CurrentPage() != MenuPage::None ||
            !SafeReadPlayerAlive()) {
            return;
        }
        if (ScriptRuntime::IsPlayerOnMission() ||
            ScriptRuntime::AlreadyRunningAMissionScript() ||
            IsIntroSequencePendingValidation() ||
            IsIntroSequenceLaunchPending()) {
            g_bootOutfitRestoreDone = false;
            g_bootOutfitSettleFrames = 0;
            return;
        }
        if (g_bootOutfitRestoreDone) {
            return;
        }
        if (PlayerVehicle()) {
            // Wait for a complete on-foot state, then give vehicle-exit
            // animations time to settle before replacing the player clump.
            g_bootOutfitSettleFrames = 0;
            return;
        }
        // Let the world settle a moment after control returns before dressing.
        if (++g_bootOutfitSettleFrames < 90) {
            return;
        }
        g_bootOutfitRestoreDone = true;
        LaunchRestorePlayerOutfitStub();
        Logger::Log("Boot outfit restore applied (free-roam, run active)");
    }

    inline void RecoverFromIntroSequenceQuit() {
        ResetIntroSequenceValidationState();
        Hooks::g_allowNextEngineIntroLaunch = false;
        Hooks::g_allowNextEngineEightballLaunch = false;
        Hooks::g_blockEngineMissionLaunches = true;
        WritePlayingIntro(false, "Intro quit recovery");
        LaunchRestorePlayerOutfitStub();
        QueueSafehouseWarp(true);
        Logger::Log("Intro quit recovery armed: restore outfit + safehouse warp");
    }

    inline bool PrimeManualIntroLaunch() {
        if (!ScriptRuntime::HasLiveScriptEngine()) {
            Logger::Log("Manual INTRO pre-launch: script engine not live");
            return false;
        }

        // Match the mission's own early setup more closely than a raw LoadScene:
        // flip the engine into intro mode now, then queue the SCM opcode so the
        // runtime also executes RemoveCurrentZonesModels before INTRO loads BET.
        const bool wroteFlag = WritePlayingIntro(true, "Manual INTRO pre-launch");

        std::uint8_t stub[16] = {};
        std::size_t n = 0;
        stub[n++] = 0x3D; stub[n++] = 0x04; // SET_INTRO_IS_PLAYING
        stub[n++] = 0x04; stub[n++] = 0x01; // TRUE
        stub[n++] = 0x4E; stub[n++] = 0x00; // TERMINATE_THIS_SCRIPT

        CRunningScript* s = LaunchScmStub(kPrepareIntroModeStubOffset,
                                          stub,
                                          n,
                                          ScmSlots::PrepareIntroModeBytes,
                                          "manual intro pre-launch");
        if (!s) {
            Logger::Log("Manual INTRO pre-launch: failed to queue SET_INTRO_IS_PLAYING stub");
            return wroteFlag;
        }

        Logger::Log("Manual INTRO pre-launch: queued SET_INTRO_IS_PLAYING stub");
        return true;
    }

}
