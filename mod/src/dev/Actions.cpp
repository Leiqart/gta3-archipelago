#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include "MinHook.h"
#include "plugin.h"
#include "CCutsceneMgr.h"
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
#include "CTheZones.h"
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
        Actions::LoadScene(place);
        bool found = false;
        float ground = CWorld::FindGroundZFor3DCoord(place.x, place.y, place.z + 2.0f, &found);
        if (found && ground > place.z + 1.0f) {
            bool floor = false;
            const float lower = CWorld::FindGroundZFor3DCoord(place.x, place.y, place.z + 0.5f, &floor);
            // A low roof over the place, like the Panlantic lift's, when a floor lies at the place's own height under it.
            if (floor && std::fabs(lower - place.z) < 1.5f) ground = lower;
        }
        if (!found) ground = CWorld::FindGroundZFor3DCoord(place.x, place.y, 1000.0f, &found);
        if (!found) {
            // No ground at all: another island's collision is not loaded yet; load it, as a mission does before it teleports there, and look again.
            const int level = CTheZones::GetLevelFromPosition(place);
            if (level != 0) Command<Commands::LOAD_COLLISION_WITH_SCREEN>(level);
            ground = CWorld::FindGroundZFor3DCoord(place.x, place.y, place.z + 2.0f, &found);
            if (!found) ground = CWorld::FindGroundZFor3DCoord(place.x, place.y, 1000.0f, &found);
        }
        // No ground at all in another part of the city whose collision is not loaded yet: the place's own height, as the sheets give it, is the ground.
        return found ? ground : place.z;
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

void Actions::PlaceAt(const CVector& place) {
    LogPlace("Placing the player at", place);
    LoadScene(place);
    Command<Commands::SET_PLAYER_COORDINATES>(CWorld::PlayerInFocus, place.x, place.y, place.z);
    LogPlace("Placed, the player is now at", PlayerPosition());
}

float Actions::Distance2D(const CVector& a, const CVector& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

void Actions::LoadScene(const CVector& place) {
    Command<Commands::SWITCH_STREAMING>(1);
    Command<Commands::LOAD_SCENE>(place.x, place.y, place.z);
}

void Actions::TeleportTo(const CVector& place) {
    // One line before each step, so that if the game stops during a teleport the log shows which step it was.
    LogPlace("Teleporting to", place);
    const float ground = GroundAt(place);
    Log::Write("Ground found at height " + std::to_string(ground) + (FindPlayerVehicle() ? ", moving the car" : ", moving the player on foot"));
    Command<Commands::SET_PLAYER_COORDINATES>(CWorld::PlayerInFocus, place.x, place.y, ground + 0.5f);
    LogPlace("Teleported, the player is now at", PlayerPosition());
}

void Actions::TeleportBeside(const CVector& entity, float distance) {
    // East, west, north or south: the first side whose ground is at the entity's height, not a roof above it or a hole below.
    const CVector sides[] = {{distance, 0.0f, 0.0f}, {-distance, 0.0f, 0.0f}, {0.0f, distance, 0.0f}, {0.0f, -distance, 0.0f}};
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
    LoadScene(to);
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
    CVector place = car->GetPosition();
    if (Distance2D(PlayerPosition(), place) > 10.0f) {
        TeleportBeside(place);
        // A car driving off would be gone again by the next step, like the ice cream van on its rounds: get in at once.
        if (car->m_vecMoveSpeed.Magnitude() * 50.0f < 2.0f) return;
        place = car->GetPosition();
    }
    LoadScene(place);
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

namespace {
    // Until this game time, the side mission buttons are held down after the game reads the pads.
    int g_subMissionButtonUntil = 0;
    // CPad::UpdatePads in gta3.exe 1.0 (plugin-sdk: plugin_III/game_III/CPad.cpp): the scripts read the buttons it writes (IS_BUTTON_PRESSED reads NewState).
    constexpr std::uintptr_t kUpdatePadsAddress = 0x492720;
    using UpdatePadsFunction = void(__cdecl*)();
    UpdatePadsFunction g_originalUpdatePads = nullptr;

    // Runs instead of CPad::UpdatePads: the real pads first, then the buttons the autopilot holds (19 is R3, 14 Square, as the job triggers read them).
    void __cdecl OurUpdatePads() {
        g_originalUpdatePads();
        if (CTimer::m_snTimeInMilliseconds >= g_subMissionButtonUntil) return;
        CPad::GetPad(0)->NewState.ShockButtonR = 255;
        CPad::GetPad(0)->NewState.ButtonSquare = 255;
    }

    // Hooks CPad::UpdatePads the first time the autopilot presses the side mission button.
    bool HookPads() {
        static bool hooked = false;
        if (hooked) return true;
        void* const target = reinterpret_cast<void*>(kUpdatePadsAddress);
        hooked = MH_CreateHook(target, reinterpret_cast<void*>(&OurUpdatePads), reinterpret_cast<void**>(&g_originalUpdatePads)) == MH_OK && MH_EnableHook(target) == MH_OK;
        Log::Write(hooked ? "Pad hook installed" : "Pad hook failed");
        return hooked;
    }

    // The car PushCar drives, as a pool handle, its speed in m/s, and the game time it stops at.
    int g_pushedCar = -1;
    float g_pushSpeed = 0.0f;
    int g_pushUntil = 0;
}

void Actions::PushCar(float speed, int milliseconds) {
    CVehicle* car = FindPlayerVehicle();
    if (!car) return;
    g_pushedCar = CPools::GetVehicleRef(car);
    g_pushSpeed = speed;
    g_pushUntil = CTimer::m_snTimeInMilliseconds + milliseconds;
    Log::Write("Driving the car ahead at " + std::to_string(static_cast<int>(speed)) + " m/s for " + std::to_string(milliseconds) + " ms");
}

void Actions::UpdatePush() {
    if (g_pushedCar < 0) return;
    CVehicle* car = CPools::GetVehicle(g_pushedCar);
    if (!car || CTimer::m_snTimeInMilliseconds > g_pushUntil) {
        g_pushedCar = -1;
        return;
    }
    // The game's own friction and brakes stop a car pushed once: the speed is set again each frame, the fall kept.
    car->bIsStatic = false;
    const CVector ahead = car->GetForward() * (g_pushSpeed / 50.0f);
    car->m_vecMoveSpeed = CVector(ahead.x, ahead.y, car->m_vecMoveSpeed.z);
}

void Actions::PressSubMissionButton() {
    if (!HookPads()) return;
    g_subMissionButtonUntil = CTimer::m_snTimeInMilliseconds + 500;
    Log::Write("Pressing the side mission button");
}

void Actions::Honk() {
    if (!FindPlayerVehicle() || !HookHorn()) return;
    // Hold the horn for one second, as a player presses it.
    g_hornUntil = CTimer::m_snTimeInMilliseconds + 1000;
    Log::Write("Honking for one second");
}

// The bit of CPad::DisablePlayerControls a cutscene holds, PLAYERCONTROL_CUTSCENE in re3.
constexpr unsigned char kCutsceneControls = 128;

void Actions::GiveControlBack() {
    // A contact's trigger switches streaming off before it launches a mission, which switches it back on in the setup a forced pass skips: LOAD_SCENE then never ends.
    Command<Commands::SWITCH_STREAMING>(1);
    Command<Commands::SET_PLAYER_CONTROL>(CWorld::PlayerInFocus, 1);
    Command<Commands::SWITCH_WIDESCREEN>(0);
    Command<Commands::SET_CAMERA_BEHIND_PLAYER>();
    Command<Commands::RESTORE_CAMERA_JUMPCUT>();
    // The screen faded out before the launch (the intro ends with DO_FADE 0, 0) is faded back in by the mission's setup, which a forced pass skips.
    Command<Commands::DO_FADE>(500, 1);
    // The trigger's MAKE_PLAYER_SAFE_FOR_CUTSCENE also starts a cutscene's processing, which only the mission's cutscene ends: until then the game loads no other part of the city's collision, and the player falls through the ground there.
    CCutsceneMgr::ms_cutsceneProcessing = false;
    CPad::GetPad(0)->DisablePlayerControls &= ~kCutsceneControls;
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

void Actions::SetScriptGlobal(int offset, int value) {
    *reinterpret_cast<int*>(CTheScripts::ScriptSpace + offset) = value;
}
