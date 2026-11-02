#include "GameRuntime.h"
#include "Connection.h"
#include "Log.h"
#include "MissionAccess.h"
#include "MissionMarkers.h"
#include "MissionProgress.h"
#include "MissionManager.h"
#include "MissionTable.h"
#include "PackageManager.h"
#include "Session.h"
#include "OpenIsland.h"
#include "UI.h"
#include "IntroSequence.h"
#include "opcode/NativeExecutor.h"
#include "../third_party/minhook/include/MinHook.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>

namespace {
    // GTA III PC 1.0 only; references are recorded in docs/archipelago-integration.md.
    constexpr std::uintptr_t kGameProcess = 0x48C850;
    constexpr std::uintptr_t kProcessCommand = 0x439500;
    constexpr std::uintptr_t kRestartWorld = 0x48C740;
    constexpr std::uintptr_t kGameTime = 0x885B48;
    constexpr std::uintptr_t kUserPause = 0x95CD7C;
    constexpr std::uintptr_t kCodePause = 0x95CDB1;
    constexpr std::uintptr_t kScriptSpace = 0x74B248;
    constexpr std::uintptr_t kMissionFlagOffset = 0x8F1B64;
    constexpr std::uintptr_t kAlreadyRunningMission = 0x95CDB3;
    constexpr std::uintptr_t kMissionsPassed = 0x940768;
    constexpr std::uintptr_t kPlayerInfo = 0x9412F0;
    constexpr std::size_t kScriptBytes = 163840;
    constexpr std::array<int, 10> kRewardOffsets = {
        4180, 4184, 4196, 4192, 4188, 4200, 4204, 4208, 4212, 4216};

    using Process = void(__cdecl*)();
    using Command = char(__thiscall*)(void*);
    Process g_process = nullptr;
    Process g_restart = nullptr;
    Command g_command = nullptr;
    std::unique_ptr<Session> g_session;
    Opcode::NativeExecutor g_opcodes;
    OpenIsland::Controller g_islands(g_opcodes);
    MissionMarkers::Controller g_markers(g_opcodes);
    IntroSequence::Controller g_intro;
    bool g_bootstrapEightSeen = false;
    bool g_installed = false;
    bool g_failedMessage = false;
    int g_previousMissionsPassed = -1;

    template <typename T> T& At(std::uintptr_t address) { return *reinterpret_cast<T*>(address); }

    bool MissionRunning() {
        // OnAMissionFlag stores a byte offset into ScriptSpace, not a boolean.
        const auto offset = At<unsigned int>(kMissionFlagOffset);
        return At<bool>(kAlreadyRunningMission) ||
            (offset > 0 && offset + sizeof(int) <= kScriptBytes && At<int>(kScriptSpace + offset) != 0);
    }

    bool PlayerReady() {
        auto* ped = At<std::uint8_t*>(kPlayerInfo);
        return ped && At<unsigned char>(kPlayerInfo + 0xD8) == 0 &&
            *reinterpret_cast<float*>(ped + 0x2C0) > 0;
    }

    bool ApplyItem(const std::string& item) {
        if (!PlayerReady()) return false;
        if (item.rfind("unlock_", 0) == 0) {
            // Island ownership is reconciled by OpenIsland after this item tick.
            Log::Write("Archipelago unlock received: %s", item.c_str());
            return true;
        }
        // Intro and mission scripts may overwrite inventory/cash while staging
        // their scene. Deliver consumables after mission cleanup instead.
        if (MissionRunning() || g_markers.Busy() || At<bool>(0x95CDC2)) return false;
        if (item == "cash") {
            const int cash = At<int>(kPlayerInfo + 0xAC);
            const int amount = cash > (std::numeric_limits<int>::max)() - 2000
                ? (std::numeric_limits<int>::max)() - cash : 2000;
            if (!Opcode::AddMoney(g_opcodes, 0, amount)) return false;
        } else if (item == "armor") {
            if (!Opcode::AddArmor(g_opcodes, 0, 100)) return false;
        } else {
            struct Weapon { const char* name; int type, ammo, model; };
            constexpr Weapon weapons[] = {
                {"weapon_bat", 1, 1, 172}, {"weapon_pistol", 2, 90, 173},
                {"weapon_uzi", 3, 240, 178}, {"weapon_shotgun", 4, 40, 174},
                {"weapon_ak47", 5, 180, 171}, {"weapon_m16", 6, 300, 180},
                {"weapon_sniper", 7, 30, 176}, {"weapon_rocket", 8, 10, 177},
                {"weapon_flamethrower", 9, 200, 175}, {"weapon_molotov", 10, 8, 181},
                {"weapon_grenade", 11, 8, 170}};
            for (const auto& weapon : weapons) {
                if (item != weapon.name) continue;
                if (!Opcode::GiveWeapon(g_opcodes, 0, weapon.type, weapon.ammo, weapon.model)) return false;
                Log::Write("Archipelago item applied: %s", item.c_str());
                return true;
            }
            Log::Write("Unsupported Archipelago item: %s", item.c_str());
            return false;
        }
        Log::Write("Archipelago item applied: %s", item.c_str());
        return true;
    }

    void MissionEvent(MissionManager::Event event, int mission) {
        if (g_session) MissionProgress::Record(*g_session, event, mission);
        if ((event == MissionManager::Event::Success || event == MissionManager::Event::Failed) &&
            g_intro.Active() && mission == g_intro.Mission()) {
            g_intro.Reset();
            if (PlayerReady() && !MissionRunning()) Opcode::RestoreFreeRoam(g_opcodes, 0);
        }
    }

    void ConsumePackageRewards() {
        if (!g_session || g_session->Seed().empty() || g_session->Faulted()) return;
        for (std::size_t i = 0; i < kRewardOffsets.size(); ++i) {
            auto& flag = At<int>(kScriptSpace + kRewardOffsets[i]);
            // Runs immediately after SET_VAR_INT, before a hideout can create
            // its vanilla weapon pickup. Do not call the counter-based detector.
            PackageManager::ConsumeReward(PackageManager::kMilestones[i].packages, flag,
                [](const char* location) { return g_session->Check(location); });
        }
    }

    char __fastcall ProcessCommand(void* script, void*) {
        auto& ip = *reinterpret_cast<unsigned int*>(static_cast<std::uint8_t*>(script) + 0x10);
        if (ip + 2 > kScriptBytes) return g_command(script);
        const auto instructionIp = ip;
        const auto rawOpcode = At<unsigned short>(kScriptSpace + ip);
        const auto opcode = static_cast<std::uint16_t>(rawOpcode & 0x7FFF);
        const auto* thread = static_cast<const std::uint8_t*>(script);
        const bool missionScript = thread[0x85] != 0 || thread[0x79] != 0;
        if (missionScript && Opcode::IsEightScript(script) && g_intro.Active()) {
            const auto action = g_intro.BeforeCommand(ip - IntroSequence::kMissionBase, opcode);
            if (action == IntroSequence::Action::Initialize) {
                Opcode::InitializeEightHandles(script);
            } else if (action == IntroSequence::Action::PrepareLuigi) {
                int player = -1, eightball = -1;
                if (Opcode::PrepareLuigisGirls(g_opcodes, player, eightball) &&
                    Opcode::EnterLuigiCutscene(script, player, eightball)) {
                    g_intro.Prepared();
                    Log::Write("Luigi's Girls: entered native LM1 cutscene without replaying GML");
                    return 0;
                }
                // End via the original cleanup caller if preparation fails.
                // Never fall through into the bridge half with partial actors.
                if (eightball >= 0) g_opcodes.Execute({Opcode::Id::DeleteChar, {eightball}});
                if (Opcode::AbortLuigiPreparation(script)) {
                    Opcode::RestoreFreeRoam(g_opcodes, 0);
                    Log::Write("Luigi's Girls preparation failed; native cleanup, no check");
                    return 0;
                }
                Log::Write("Luigi's Girls preparation rejected: unexpected script return stack");
                return 1;
            } else if (action == IntroSequence::Action::FinishGml) {
                const int car = Opcode::EightCarHandle();
                if (Opcode::ReturnFromGiveMeLiberty(script)) {
                    Opcode::ReleaseMissionCar(g_opcodes, car);
                    Opcode::RestoreFreeRoam(g_opcodes, 0);
                    g_intro.FinishedGml();
                    MissionManager::ConfirmScriptedSuccess(IntroSequence::kGiveMeLiberty);
                    Log::Write("Give Me Liberty: hideout finished; native cleanup before separate Luigi mission");
                    return 0;
                }
                return 1;
            } else if (action == IntroSequence::Action::SkipCriticalRestart) {
                Opcode::Parameters restart;
                if (Opcode::ReadParameters(script, ip + 2, 4, restart)) {
                    ip = restart.nextIp;
                    Log::Write("EIGHT failure: suppressed critical restart into GML; returning to AP marker");
                    return 0;
                }
            }
        }
        bool blockLocate = false, trackBlip = false, changedBlip = false;
        Opcode::Parameters parameters;
        const short locateCount = MissionMarkers::LocateParameterCount(rawOpcode);
        if (locateCount && instructionIp < 128 * 1024 && !missionScript && !(rawOpcode & 0x8000) &&
            Opcode::ReadParameters(script, ip + 2, locateCount, parameters)) {
            const Opcode::Point target{parameters.Float(1), parameters.Float(2), 0};
            const auto* contact = MissionMarkers::ContactAt(target);
            // The bridge trigger relaunches EIGHT after a critical restart; keep it while vanilla owns Give Me Liberty.
            const bool vanillaEightTrigger = contact && std::strcmp(contact->chain, "intro") == 0 &&
                IntroSequence::VanillaOwnsEight(g_bootstrapEightSeen, g_session && g_session->IsChecked("mission_give_me_liberty"));
            blockLocate = parameters.values[0] == 0 &&
                !vanillaEightTrigger && MissionMarkers::SuppressVanillaLocate(instructionIp, missionScript, rawOpcode, target);
        } else if ((opcode == static_cast<unsigned short>(Opcode::Id::AddContactBlip) ||
                    opcode == static_cast<unsigned short>(Opcode::Id::AddCoordinateBlip)) &&
                   Opcode::ReadParameters(script, ip + 2, 4, parameters)) {
            trackBlip = MissionMarkers::ContactAt({parameters.Float(0), parameters.Float(1), parameters.Float(2)}) != nullptr;
        } else if (opcode == static_cast<unsigned short>(Opcode::Id::ChangeBlipDisplay) &&
                   Opcode::ReadParameters(script, ip + 2, 2, parameters)) {
            changedBlip = g_markers.IsVanillaBlip(parameters.values[0]);
        }
        if (opcode == 0x0417) {
            Opcode::ReadParameters(script, ip + 2, 1, parameters);
            const int nativeMission = parameters.values[0];
            // Native EIGHT is Give Me Liberty for the intro bootstrap and every vanilla restart until it is checked.
            const bool vanillaEight = nativeMission == 19 &&
                IntroSequence::VanillaOwnsEight(g_bootstrapEightSeen, g_session && g_session->IsChecked("mission_give_me_liberty"));
            const int mission = vanillaEight ? IntroSequence::kGiveMeLiberty : MissionTable::LogicalIndex(nativeMission);
            const auto* location = MissionTable::LocationFor(mission);
            // Only bootstrap waits here. Street triggers are blocked at LOCATE,
            // and AP-owned launches run through a private wrapper script.
            if ((nativeMission == 0 || vanillaEight) && (!g_session || g_session->Seed().empty() || g_session->Faulted())) return 1;
            const bool denied = !vanillaEight && location && (MissionMarkers::OwnsMission(mission) || !g_session ||
                !MissionAccess::CanStart(*g_session, location) || g_session->IsChecked(location));
            if (denied) {
                // Defensive fallback for an unfamiliar vanilla launch path:
                // consume the instruction and restore staging instead of freezing.
                ip = parameters.nextIp;
                if (!At<bool>(kAlreadyRunningMission)) {
                    const auto offset = At<unsigned int>(kMissionFlagOffset);
                    if (offset > 0 && offset <= kScriptBytes - sizeof(int)) At<int>(kScriptSpace + offset) = 0;
                    Opcode::RestoreFreeRoam(g_opcodes, 0);
                }
                Log::Write("Vanilla mission %d launch suppressed; AP contact markers own access", mission);
                return 1;
            }
            if (vanillaEight) {
                g_bootstrapEightSeen = true;
                g_intro.Begin(IntroSequence::kGiveMeLiberty);
            }
            MissionManager::NotifyMissionLaunch(mission);
            MissionManager::Update(true, At<int>(kMissionsPassed));
        }
        const char result = g_command(script);
        if (opcode == 0x03A4 && missionScript && Opcode::IsEightScript(script))
            Log::Write("EIGHT native script entry IP=%u; expected=%u", instructionIp, IntroSequence::kMissionBase);
        if (blockLocate) Opcode::SetCompareResult(script, false);
        if (trackBlip) {
            int handle = -1;
            if (Opcode::ReadOutputVariable(script, parameters.nextIp, handle)) g_markers.TrackVanillaBlip(handle);
        }
        if (changedBlip) g_markers.VanillaBlipChanged(parameters.values[0]);
        if (opcode == 0x0004) ConsumePackageRewards();
        return result;
    }

    MissionMarkers::Frame MarkerFrame() {
        MissionMarkers::Frame frame;
        frame.paused = At<bool>(kUserPause) || At<bool>(kCodePause);
        frame.worldReady = PlayerReady() && !At<bool>(0x95CDC2) && g_bootstrapEightSeen;
        frame.missionRunning = MissionRunning();
        if (frame.worldReady && !frame.paused) {
            if (!Opcode::GetPlayerPosition(g_opcodes, 0, frame.position)) frame.worldReady = false;
            Opcode::Fading(g_opcodes, frame.fading);
            if (!frame.missionRunning) {
                Opcode::PlayerOnFoot(g_opcodes, 0, frame.onFoot);
                Opcode::PlayerCanStartMission(g_opcodes, 0, frame.canStart);
            }
        }
        return frame;
    }

    void __cdecl RestartWorld() {
        // InitialiseWhenRestarting is shared by New Game and load. Never use old
        // radar handles or a pending fade in the rebuilt world.
        g_markers.Reset();
        g_islands.Reset();
        MissionManager::Reset();
        g_intro.Reset();
        g_bootstrapEightSeen = false;
        g_previousMissionsPassed = -1;
        g_restart();
    }

    void __cdecl ProcessGame() {
        // Poll before scripts so a snapshot can unblock a waiting launch this frame.
        GameRuntime::Tick();
        g_process();
        if (g_session && !g_session->Seed().empty() && !g_session->Faulted()) {
            const int passed = At<int>(kMissionsPassed);
            if (g_previousMissionsPassed >= 0 && passed < g_previousMissionsPassed)
                MissionManager::Reset();
            g_previousMissionsPassed = passed;
            MissionManager::Update(MissionRunning(), passed);
            ConsumePackageRewards();
            g_markers.Render(*g_session, MarkerFrame());
        }
    }

    bool SupportedExecutable() {
        // Refuse different layouts before touching a game address. Same version
        // probe as plugin-sdk; addresses assume the original image base.
        const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
        if (base != 0x400000) return false;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->OptionalHeader.SizeOfImage <= 0x55CE00) return false;
        return At<unsigned int>(0x5C1E70) == 0x53E58955;
    }
}

bool GameRuntime::Install(HMODULE module) {
    if (g_installed) return true;
    if (!SupportedExecutable()) {
        Log::Write("Game hooks disabled: GTA III PC 1.0 x86 executable required");
        return false;
    }
    char executable[MAX_PATH]{};
    GetModuleFileNameA(nullptr, executable, MAX_PATH);
    if (!IntroSequence::SupportedScript(std::filesystem::path(executable).parent_path() / "data" / "main.scm")) {
        Log::Write("Game hooks disabled: unsupported main.scm; verified vanilla script required");
        return false;
    }
    char path[MAX_PATH]{};
    GetModuleFileNameA(module, path, MAX_PATH);
    g_session = std::make_unique<Session>([](const Session::Json& message) {
        Connection::Send(message.dump());
    }, (std::filesystem::path(path).parent_path() / "GTA3Archipelago-state").string());
    MissionManager::Reset();
    g_islands.Reset();
    g_markers.Reset();
    MissionManager::SetEventSink(MissionEvent);
    if (MH_Initialize() != MH_OK) return false;
    if (MH_CreateHook(reinterpret_cast<void*>(kGameProcess), &ProcessGame,
                      reinterpret_cast<void**>(&g_process)) != MH_OK ||
        MH_CreateHook(reinterpret_cast<void*>(kProcessCommand), &ProcessCommand,
                      reinterpret_cast<void**>(&g_command)) != MH_OK ||
        MH_CreateHook(reinterpret_cast<void*>(kRestartWorld), &RestartWorld,
                      reinterpret_cast<void**>(&g_restart)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(kRestartWorld)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(kProcessCommand)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void*>(kGameProcess)) != MH_OK) {
        MH_Uninitialize();
        MissionManager::SetEventSink(nullptr);
        return false;
    }
    if (!UI::Install()) {
        MH_Uninitialize();
        MissionManager::SetEventSink(nullptr);
        Log::Write("Game hooks disabled: Archipelago UI installation failed");
        return false;
    }
    g_installed = true;
    Log::Write("GTA III 1.0 game and script hooks installed");
    return true;
}

void GameRuntime::Tick() {
    if (!g_session) return;
    if (!Connection::ProtocolReady()) g_session->Disconnected();
    for (const auto& line : Connection::DrainMessages()) {
        const auto message = Session::Json::parse(line, nullptr, false);
        if (message.is_discarded() || !g_session->Receive(message)) {
            g_session->Disconnected();
            Connection::Reconnect();
            Log::Write("Archipelago snapshot rejected; no further items applied until resync");
            break;
        }
    }
    if (g_session->Faulted()) {
        if (!g_failedMessage) Log::Write("Archipelago stopped: inspect room identity and state journal");
        g_failedMessage = true;
        g_markers.Update(*g_session, MarkerFrame(), At<unsigned int>(kGameTime));
        return;
    }
    g_session->Tick(ApplyItem);
    const auto markerFrame = MarkerFrame();
    const int launched = g_markers.Update(*g_session, markerFrame, At<unsigned int>(kGameTime));
    if (launched >= 0) {
        g_intro.Begin(launched);
        MissionManager::NotifyMissionLaunch(launched);
        MissionManager::Update(true, At<int>(kMissionsPassed));
    }
    g_islands.Update(
        g_session->HasItem("unlock_commercial") || g_session->IsChecked("mission_frank4"),
        g_session->HasItem("unlock_suburban") || g_session->IsChecked("mission_love3"),
        markerFrame.worldReady && !markerFrame.paused && !MissionRunning() && !g_markers.Busy(), GetTickCount64());
}

void GameRuntime::Shutdown() {
    if (g_installed) {
        UI::Shutdown();
        MH_DisableHook(reinterpret_cast<void*>(kGameProcess));
        MH_DisableHook(reinterpret_cast<void*>(kProcessCommand));
        MH_DisableHook(reinterpret_cast<void*>(kRestartWorld));
        MH_Uninitialize();
        g_installed = false;
    }
    MissionManager::SetEventSink(nullptr);
    g_session.reset();
    g_islands.Reset();
    g_markers.Reset();
}
