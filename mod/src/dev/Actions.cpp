#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include "MinHook.h"
#include "plugin.h"
#include "CMessages.h"
#include "CObject.h"
#include "CPad.h"
#include "CTimer.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CProjectileInfo.h"
#include "CRunningScript.h"
#include "CTheScripts.h"
#include "CVehicle.h"
#include "CWorld.h"
#include "common.h"
#include "eModelID.h"
#include "extensions/ScriptCommands.h"
#include "../Log.h"
#include "Actions.h"

using namespace plugin;

namespace {
    // True for a blip the mission placed: map icons (shops, safehouses) have a radar sprite, mission blips have none.
    bool IsMissionBlip(const tRadarTrace& blip, eBlipType type) {
        return blip.m_bInUse && blip.m_nRadarSprite == 0 && blip.m_nBlipType == type;
    }

    // The ground height at a place, looked for from 2 m above it so that a roof or an awning over it is not picked; loads the area first.
    float GroundAt(const CVector& place) {
        Command<Commands::LOAD_SCENE>(place.x, place.y, place.z);
        bool found = false;
        const float ground = CWorld::FindGroundZFor3DCoord(place.x, place.y, place.z + 2.0f, &found);
        return found ? ground : CWorld::FindGroundZForCoord(place.x, place.y);
    }

    // Until this game time, CPad::GetHorn answers yes: the autopilot holds the horn.
    int g_hornUntil = 0;
    // CPad::GetHorn in gta3.exe 1.0 (plugin-sdk: plugin_III/game_III/CPad.cpp): the vehicle code and IS_PLAYER_PRESSING_HORN both ask it.
    constexpr std::uintptr_t kGetHornAddress = 0x493350;
    using GetHornFunction = bool(__thiscall*)(CPad* pad);
    GetHornFunction g_originalGetHorn = nullptr;

    // Runs instead of CPad::GetHorn: yes while the autopilot holds the horn, else the player's real button.
    bool __fastcall OurGetHorn(CPad* pad, void*) {
        if (CTimer::m_snTimeInMilliseconds < g_hornUntil) return true;
        return g_originalGetHorn(pad);
    }

    // Hooks CPad::GetHorn the first time the autopilot honks; MinHook was started by the mod thread long before any mission.
    bool HookHorn() {
        static bool hooked = false;
        if (hooked) return true;
        void* const target = reinterpret_cast<void*>(kGetHornAddress);
        hooked = MH_CreateHook(target, reinterpret_cast<void*>(&OurGetHorn), reinterpret_cast<void**>(&g_originalGetHorn)) == MH_OK && MH_EnableHook(target) == MH_OK;
        Log::Write(hooked ? "Horn hook installed" : "Horn hook failed");
        return hooked;
    }

    // Writes one line of the log with a place, like "Teleported to 875.0, -309.0, 7.7".
    void LogPlace(const std::string& what, const CVector& place) {
        Log::Write(what + " " + std::to_string(place.x) + ", " + std::to_string(place.y) + ", " + std::to_string(place.z));
    }

}

void Actions::LogPlaceAndHeading(const std::string& what, CPlaceable& placeable) {
    float degrees = placeable.GetHeading() * 180.0f / 3.14159265f;
    if (degrees < 0.0f) degrees += 360.0f;
    LogPlace(what, placeable.GetPosition());
    Log::Write("  heading " + std::to_string(degrees) + " degrees");
}

void Actions::Say(const wchar_t* text) {
    CMessages::AddMessageJumpQ(text, 2000, 0);
}

const tRadarTrace* Actions::MissionBlip(bool withPickups) {
    for (eBlipType type : {BLIP_COORD, BLIP_CHAR, BLIP_CAR, BLIP_OBJECT}) {
        if (type == BLIP_OBJECT && !withPickups) break;
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (IsMissionBlip(blip, type)) return &blip;
        }
    }
    return nullptr;
}

bool Actions::BlipPosition(const tRadarTrace& blip, CVector& position) {
    CEntity* entity = nullptr;
    switch (blip.m_nBlipType) {
        case BLIP_COORD: position = blip.m_vecPos; return true;
        case BLIP_CHAR: entity = CPools::GetPed(blip.m_nEntityHandle); break;
        case BLIP_CAR: entity = CPools::GetVehicle(blip.m_nEntityHandle); break;
        case BLIP_OBJECT: entity = CPools::GetObject(blip.m_nEntityHandle); break;
        default: break;
    }
    if (!entity) return false;
    position = entity->GetPosition();
    return true;
}

CVector Actions::PlayerPosition() {
    // FindPlayerCoors follows m_pVehicle whenever m_bInVehicle is set, without checking that the car still exists; we check both.
    CPlayerPed* player = FindPlayerPed();
    if (player->m_bInVehicle && player->m_pVehicle) return player->m_pVehicle->GetPosition();
    return player->GetPosition();
}

float Actions::Distance2D(const CVector& a, const CVector& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

void Actions::TeleportTo(const CVector& place) {
    // One line before each step, so that if the game stops during a teleport the log shows which step it was.
    LogPlace("Teleporting to", place);
    const float ground = GroundAt(place);
    Log::Write("Ground found at height " + std::to_string(ground) + (FindPlayerVehicle() ? ", moving the car" : ", moving the player on foot"));
    Command<Commands::SET_PLAYER_COORDINATES>(CWorld::PlayerInFocus, place.x, place.y, ground + 0.5f);
    LogPlace("Teleported, the player is now at", PlayerPosition());
}

void Actions::TeleportBeside(const CVector& entity) {
    // 4 m east, west, north or south: the first side whose ground is at the entity's height, not a roof above it or a hole below.
    const CVector sides[] = {{4.0f, 0.0f, 0.0f}, {-4.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}, {0.0f, -4.0f, 0.0f}};
    for (const CVector& side : sides) {
        const CVector place = entity + side;
        if (std::fabs(GroundAt(place) - entity.z) < 2.0f) {
            TeleportTo(place);
            return;
        }
    }
    LogPlace("No side at the same height, teleporting onto", entity);
    TeleportTo(entity);
}

bool Actions::GoToMissionBlip() {
    const tRadarTrace* blip = MissionBlip(true);
    CVector target;
    if (!blip || !BlipPosition(*blip, target)) return false;
    // Beside a character, car or pickup, not inside it.
    if (blip->m_nBlipType == BLIP_COORD) TeleportTo(target);
    else TeleportBeside(target);
    return true;
}

int Actions::KillMarkedCharacters() {
    int killed = 0;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        if (!IsMissionBlip(blip, BLIP_CHAR) || !CPools::GetPed(blip.m_nEntityHandle) || Command<Commands::IS_CHAR_DEAD>(blip.m_nEntityHandle)) continue;
        // EXPLODE_CHAR_HEAD kills without dealing damage, so it works even on the characters a mission makes only the player can hurt.
        Command<Commands::EXPLODE_CHAR_HEAD>(blip.m_nEntityHandle);
        ++killed;
    }
    if (killed > 0) Log::Write("Killed " + std::to_string(killed) + " marked character(s)");
    return killed;
}

int Actions::ExplodeMarkedCars() {
    int exploded = 0;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        if (!IsMissionBlip(blip, BLIP_CAR) || !CPools::GetVehicle(blip.m_nEntityHandle) || Command<Commands::IS_CAR_DEAD>(blip.m_nEntityHandle)) continue;
        Command<Commands::EXPLODE_CAR>(blip.m_nEntityHandle);
        ++exploded;
    }
    if (exploded > 0) Log::Write("Blew up " + std::to_string(exploded) + " marked car(s)");
    return exploded;
}

int Actions::KillMarkedCarOccupants() {
    int killed = 0;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        if (!IsMissionBlip(blip, BLIP_CAR)) continue;
        CVehicle* car = CPools::GetVehicle(blip.m_nEntityHandle);
        if (!car) continue;
        std::vector<CPed*> occupants{car->m_pDriver};
        for (CPed* passenger : car->m_apPassengers) occupants.push_back(passenger);
        for (CPed* ped : occupants) {
            if (!ped || ped == FindPlayerPed()) continue;
            const int handle = CPools::GetPedRef(ped);
            if (Command<Commands::IS_CHAR_DEAD>(handle)) continue;
            Command<Commands::EXPLODE_CHAR_HEAD>(handle);
            ++killed;
        }
    }
    if (killed > 0) Log::Write("Killed " + std::to_string(killed) + " occupant(s) of the marked car(s)");
    return killed;
}

const tRadarTrace* Actions::NearestMissionBlip(eBlipType type) {
    const tRadarTrace* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        CVector position;
        if (!IsMissionBlip(blip, type) || !BlipPosition(blip, position)) continue;
        const float distance = Distance2D(PlayerPosition(), position);
        if (!nearest || distance < nearestDistance) {
            nearest = &blip;
            nearestDistance = distance;
        }
    }
    return nearest;
}

bool Actions::MissionBlipNear(const CVector& place, float distance) {
    for (eBlipType type : {BLIP_COORD, BLIP_CHAR, BLIP_CAR, BLIP_OBJECT}) {
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            CVector position;
            if (IsMissionBlip(blip, type) && BlipPosition(blip, position) && Distance2D(position, place) < distance) return true;
        }
    }
    return false;
}

void Actions::ThrowGrenade(const CVector& place) {
    CProjectileInfo::AddProjectile(FindPlayerPed(), WEAPONTYPE_GRENADE, place, 0.0f);
    LogPlace("Dropped a grenade at", place);
}

bool Actions::GroupMemberOnFoot() {
    CPlayerPed* player = FindPlayerPed();
    auto* pool = CPools::ms_pPedPool;
    for (int i = 0; i < pool->m_nSize; ++i) {
        CPed* ped = pool->GetAt(i);
        if (ped && ped != player && ped->m_pGangLeader == player && !ped->m_bInVehicle && !Command<Commands::IS_CHAR_DEAD>(CPools::GetPedRef(ped))) return true;
    }
    return false;
}

void Actions::GiveCar(int model) {
    const CVector position = PlayerPosition();
    Command<Commands::REQUEST_MODEL>(model);
    Command<Commands::LOAD_ALL_MODELS_NOW>();
    int car = 0;
    Command<Commands::CREATE_CAR>(model, position.x + 3.0f, position.y, position.z, &car);
    Command<Commands::WARP_PLAYER_INTO_CAR>(CWorld::PlayerInFocus, car);
    // Hand the car and its model back to the game, like a mission does once it no longer needs them.
    Command<Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(car);
    Command<Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    Log::Write("Gave a car of model " + std::to_string(model));
}

void Actions::TeleportCarrying(const CVector& place, const std::vector<int>& charHandles) {
    CVehicle* car = FindPlayerVehicle();
    if (!car) return;
    const CVector from = car->GetPosition();
    // A boat stays at the water's height: the ground under it is the sea floor.
    const CVector to{place.x, place.y, from.z};
    std::vector<std::pair<CPed*, CVector>> carried;
    for (int handle : charHandles) {
        if (CPed* ped = CPools::GetPed(handle)) carried.emplace_back(ped, ped->GetPosition() - from);
    }
    LogPlace("Teleporting the boat and " + std::to_string(carried.size()) + " character(s) on it to", to);
    Command<Commands::LOAD_SCENE>(to.x, to.y, to.z);
    Command<Commands::SET_PLAYER_COORDINATES>(CWorld::PlayerInFocus, to.x, to.y, to.z);
    for (auto& [ped, offset] : carried) ped->Teleport(to + offset);
    LogPlace("Teleported, the player is now at", PlayerPosition());
}

void Actions::EnterCar(int carHandle) {
    CVehicle* car = CPools::GetVehicle(carHandle);
    if (!car) {
        Log::Write("No car for the blip handle " + std::to_string(carHandle));
        return;
    }
    if (FindPlayerVehicle() == car) return;
    if (FindPlayerVehicle()) {
        LeaveCarTo(car->GetPosition() + CVector(4.0f, 0.0f, 0.0f));
        return;
    }
    // A far car sits in an area the game has not loaded: walking up beside it first, then getting in on the next step, is what a mission expects.
    const CVector place = car->GetPosition();
    if (Distance2D(PlayerPosition(), place) > 10.0f) {
        TeleportBeside(place);
        return;
    }
    Command<Commands::LOAD_SCENE>(place.x, place.y, place.z);
    Command<Commands::WARP_PLAYER_INTO_CAR>(CWorld::PlayerInFocus, carHandle);
    LogPlace("Entered the car at", place);
}

void Actions::LogWhereIAm() {
    if (CVehicle* car = FindPlayerVehicle()) {
        LogPlaceAndHeading("Where I am: in a car at", *car);
        // m_vecMoveSpeed is in metres per 1/50 s, the game's time step unit.
        Log::Write("  speed " + std::to_string(car->m_vecMoveSpeed.Magnitude() * 50.0f) + " m/s");
    }
    else LogPlaceAndHeading("Where I am: on foot at", *FindPlayerPed());
}

void Actions::SetCarHeading(float degrees) {
    CVehicle* car = FindPlayerVehicle();
    if (!car) return;
    Command<Commands::SET_CAR_HEADING>(CPools::GetVehicleRef(car), degrees);
    Log::Write("Turned the car to heading " + std::to_string(degrees));
}

void Actions::Honk() {
    if (!FindPlayerVehicle() || !HookHorn()) return;
    // Hold the horn for one second, as a player presses it.
    g_hornUntil = CTimer::m_snTimeInMilliseconds + 1000;
    Log::Write("Honking for one second");
}

void Actions::GiveControlBack() {
    // A contact's trigger switches streaming off before it launches a mission, which switches it back on in the setup a forced pass skips: LOAD_SCENE then never ends.
    Command<Commands::SWITCH_STREAMING>(1);
    Command<Commands::SET_PLAYER_CONTROL>(CWorld::PlayerInFocus, 1);
    Command<Commands::SWITCH_WIDESCREEN>(0);
    Command<Commands::SET_CAMERA_BEHIND_PLAYER>();
    Command<Commands::RESTORE_CAMERA_JUMPCUT>();
    Log::Write("Gave the player control, the camera and streaming back");
}

void Actions::DressPlayer(const char* model) {
    const int player = CPools::GetPedRef(FindPlayerPed());
    Command<Commands::UNDRESS_CHAR>(player, model);
    Command<Commands::LOAD_ALL_MODELS_NOW>();
    Command<Commands::DRESS_CHAR>(player);
    Log::Write(std::string("Dressed Claude in ") + model);
}

void Actions::LeaveCarTo(const CVector& place) {
    Command<Commands::WARP_PLAYER_FROM_CAR_TO_COORD>(CWorld::PlayerInFocus, place.x, place.y, GroundAt(place) + 0.5f);
    LogPlace("Left the car at", place);
}

const char* Actions::RunningMission() {
    for (CRunningScript* script = CTheScripts::pActiveScripts; script; script = script->m_pNext) {
        if (script->m_bIsMission) return script->m_szName;
    }
    return nullptr;
}

int Actions::ScriptGlobal(int offset) {
    return *reinterpret_cast<int*>(CTheScripts::ScriptSpace + offset);
}
