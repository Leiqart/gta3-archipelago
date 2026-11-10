#include <cmath>
#include <cstdio>
#include <utility>
#include "plugin.h"
#include "CDarkel.h"
#include "CGarage.h"
#include "CGarages.h"
#include "CFireManager.h"
#include "CHeli.h"
#include "CObject.h"
#include "CPhoneInfo.h"
#include "CPacManPickups.h"
#include "CPickups.h"
#include "CPlane.h"
#include "CPlayerPed.h"
#include "CPools.h"
#include "CVehicle.h"
#include "CWorld.h"
#include "common.h"
#include "extensions/ScriptCommands.h"
#include "../Log.h"
#include "Actions.h"
#include "Steps.h"

using namespace plugin;

namespace {
    // The car the script keeps in a global, or nullptr once it is gone: a mission stores its cars' pool handles in its globals.
    CVehicle* ScriptCar(int carGlobal) {
        return CPools::GetVehicle(Actions::ScriptGlobal(carGlobal));
    }
}

bool Steps::InCar() {
    return FindPlayerVehicle() != nullptr;
}

bool Steps::OnFoot() {
    return FindPlayerVehicle() == nullptr;
}

bool Steps::NoBlip() {
    return Actions::MissionBlip(false) == nullptr;
}

Steps::Goal Steps::BlipIs(eBlipType type) {
    return [type] {
        const tRadarTrace* blip = Actions::MissionBlip(false);
        return blip && blip->m_nBlipType == type;
    };
}

Steps::Goal Steps::BlipNear(CVector place) {
    return [place] {
        const tRadarTrace* blip = Actions::MissionBlip(false);
        CVector position;
        return blip && Actions::BlipPosition(*blip, position) && Actions::Distance2D(position, place) < 20.0f;
    };
}

Steps::Goal Steps::PlayerNear(CVector place, float distance) {
    return [place, distance] { return Actions::Distance2D(Actions::PlayerPosition(), place) < distance; };
}

Steps::Goal Steps::GarageOpen(int garageGlobal) {
    return [garageGlobal] { return CGarages::IsGarageOpen(static_cast<short>(Actions::ScriptGlobal(garageGlobal))); };
}

Steps::Goal Steps::CarInGarage(int garageGlobal) {
    return [garageGlobal] {
        CVehicle* car = FindPlayerVehicle();
        return car && CGarages::aGarages[Actions::ScriptGlobal(garageGlobal)].IsEntityEntirelyInside(car);
    };
}

Steps::Goal Steps::CarArmedWith(int carGlobal, int bombType) {
    return [carGlobal, bombType] { return ScriptCar(carGlobal) && Command<Commands::IS_CAR_ARMED_WITH_BOMB>(Actions::ScriptGlobal(carGlobal), bombType); };
}

Steps::Goal Steps::CarHealthAtMost(int carGlobal, int health) {
    return [carGlobal, health] { return ScriptCar(carGlobal) && !Command<Commands::IS_CAR_HEALTH_GREATER>(Actions::ScriptGlobal(carGlobal), health); };
}

Steps::Goal Steps::CarStoppedIn(int carGlobal, CVector corner, CVector oppositeCorner) {
    return [carGlobal, corner, oppositeCorner] {
        return ScriptCar(carGlobal) && Command<Commands::IS_CAR_STOPPED_IN_AREA_3D>(Actions::ScriptGlobal(carGlobal), corner.x, corner.y, corner.z, oppositeCorner.x, oppositeCorner.y, oppositeCorner.z, 0);
    };
}

Steps::Goal Steps::CarEmpty(int carGlobal) {
    return [carGlobal] {
        CVehicle* car = ScriptCar(carGlobal);
        return car && !car->m_pDriver && car->m_nNumPassengers == 0;
    };
}

Steps::Goal Steps::CharOnFoot(int charGlobal) {
    return [charGlobal] { return CPools::GetPed(Actions::ScriptGlobal(charGlobal)) && !Command<Commands::IS_CHAR_IN_ANY_CAR>(Actions::ScriptGlobal(charGlobal)); };
}

Steps::Goal Steps::PassengersAtLeast(int count) {
    return [count] {
        CVehicle* car = FindPlayerVehicle();
        return car && car->m_nNumPassengers >= count;
    };
}

Steps::Goal Steps::GlobalAtLeast(int global, int value) {
    return [global, value] { return Actions::ScriptGlobal(global) >= value; };
}

Steps::Goal Steps::AllOf(Goal first, Goal second) {
    return [first = std::move(first), second = std::move(second)] { return first() && second(); };
}

Steps::Goal Steps::AnyOf(Goal first, Goal second) {
    return [first = std::move(first), second = std::move(second)] { return first() || second(); };
}

Steps::Goal Steps::Not(Goal goal) {
    return [goal = std::move(goal)] { return !goal(); };
}

Steps::Goal Steps::AnyBlipNear(CVector place) {
    return [place] { return Actions::MissionBlipNear(place, 20.0f); };
}

Steps::Goal Steps::Marked(eBlipType type) {
    return [type] { return Actions::NearestMissionBlip(type) != nullptr; };
}

Steps::Goal Steps::InModel(int model) {
    return [model] { return FindPlayerVehicle() && Command<Commands::IS_PLAYER_IN_MODEL>(CWorld::PlayerInFocus, model); };
}

Steps::Goal Steps::InScriptCar(int carGlobal) {
    return [carGlobal] {
        CVehicle* car = ScriptCar(carGlobal);
        return car && FindPlayerVehicle() == car;
    };
}

bool Steps::Frenzy() {
    return CDarkel::FrenzyOnGoing();
}

Steps::Goal Steps::AnyCharNear(std::vector<int> charGlobals, int carGlobal, float distance) {
    return [charGlobals = std::move(charGlobals), carGlobal, distance] {
        CVehicle* car = ScriptCar(carGlobal);
        if (!car) return false;
        for (int global : charGlobals) {
            const int handle = Actions::ScriptGlobal(global);
            CPed* ped = CPools::GetPed(handle);
            if (ped && !Command<Commands::IS_CHAR_DEAD>(handle) && Actions::Distance2D(ped->GetPosition(), car->GetPosition()) < distance) return true;
        }
        return false;
    };
}

Steps::Action Steps::Both(Action first, Action second) {
    return [first = std::move(first), second = std::move(second)] {
        first();
        second();
    };
}

Steps::Action Steps::Either(Goal when, Action then, Action otherwise) {
    return [when = std::move(when), then = std::move(then), otherwise = std::move(otherwise)] {
        if (when()) then();
        else otherwise();
    };
}

void Steps::EnterBlipCar() {
    const tRadarTrace* blip = Actions::MissionBlip(false);
    if (blip && blip->m_nBlipType == BLIP_CAR) Actions::EnterCar(blip->m_nEntityHandle);
}

void Steps::GoToBlip() {
    const tRadarTrace* blip = Actions::MissionBlip(false);
    CVector target;
    if (!blip || !Actions::BlipPosition(*blip, target)) return;
    // Once there, the mission needs a moment to see the player stopped on its marker; beside a character or car is 4 m away.
    const bool place = blip->m_nBlipType == BLIP_COORD;
    if (Actions::Distance2D(Actions::PlayerPosition(), target) < (place ? 2.0f : 8.0f)) return;
    if (place) Actions::TeleportTo(target);
    else Actions::TeleportBeside(target);
}

Steps::Action Steps::GoTo(CVector place) {
    return [place] {
        if (Actions::Distance2D(Actions::PlayerPosition(), place) > 2.0f) Actions::TeleportTo(place);
    };
}

Steps::Action Steps::StopAt(CVector place) {
    return [place] {
        if (Actions::Distance2D(Actions::PlayerPosition(), place) > 1.0f) Actions::TeleportTo(place);
        ClearWanted();
    };
}

Steps::Action Steps::WalkTo(CVector place) {
    return [place] {
        if (FindPlayerVehicle()) Actions::LeaveCarTo(place);
        else if (Actions::Distance2D(Actions::PlayerPosition(), place) > 1.0f) Actions::TeleportTo(place);
    };
}

Steps::Action Steps::AnswerPhone(CVector place) {
    return [place] {
        if (bPickingUpPhone) return;
        CPhone* phone = nullptr;
        for (int i = 0; i < gPhoneInfo.m_nScriptPhonesMax; i++) {
            CPhone& candidate = gPhoneInfo.m_aPhones[i];
            if (candidate.m_pEntity && (!phone || Actions::Distance2D(candidate.m_vecPos, place) < Actions::Distance2D(phone->m_vecPos, place))) phone = &candidate;
        }
        if (!phone) return;
        // The game lifts a ringing phone for a player on foot within 1 m of it and in front of it (CPhoneInfo::Update), never one standing right on it.
        const CVector front = phone->m_vecPos - phone->m_pEntity->GetForward() * 0.6f;
        if (FindPlayerVehicle()) Actions::LeaveCarTo(front);
        else if (Actions::Distance2D(Actions::PlayerPosition(), front) > 0.3f) Actions::TeleportTo(front);
    };
}

Steps::Action Steps::LogGlobals(std::vector<int> globals) {
    return [globals, last = std::string()]() mutable {
        std::string values;
        for (int global : globals) values += " g" + std::to_string(global) + " = " + std::to_string(Actions::ScriptGlobal(global));
        if (values == last) return;
        Log::Write("Script globals:" + values);
        last = values;
    };
}

Steps::Action Steps::FaceHeading(float degrees) {
    return [degrees] {
        if (FindPlayerVehicle()) return;
        Command<Commands::SET_PLAYER_HEADING>(CWorld::PlayerInFocus, degrees);
        Command<Commands::SET_CAMERA_BEHIND_PLAYER>();
    };
}

Steps::Action Steps::StandAt(CVector place) {
    return [place] {
        if (FindPlayerVehicle()) Actions::LeaveCarTo(place);
        const CVector at = Actions::PlayerPosition();
        if (Actions::Distance2D(at, place) > 1.0f || std::fabs(at.z - place.z) > 1.5f) Actions::PlaceAt(place);
    };
}

Steps::Action Steps::ShortenTimer(int timerGlobal, int milliseconds) {
    return [timerGlobal, milliseconds] {
        if (Actions::ScriptGlobal(timerGlobal) <= milliseconds) return;
        Actions::SetScriptGlobal(timerGlobal, milliseconds);
        Log::Write("Moved the countdown in g" + std::to_string(timerGlobal) + " on to " + std::to_string(milliseconds / 1000) + " s");
    };
}

Steps::Action Steps::ShootDownDrugPlane(CVector place, float distance) {
    return [place, distance] {
        CPlane* plane = pDrugRunCesna;
        if (!plane || plane->m_bHasBeenHit || Actions::Distance2D(plane->GetPosition(), place) > distance) return;
        CVector at = plane->GetPosition();
        // What a rocket exploding within 25 m of it does (CPlane::TestRocketCollision): the plane is hit and crashes.
        if (CPlane::TestRocketCollision(&at)) Log::Write("Shot the drug plane down, as a rocket would");
    };
}

Steps::Action Steps::ShootDownScriptHeli(int heliGlobal) {
    return [heliGlobal] {
        CVehicle* heli = ScriptCar(heliGlobal);
        if (!heli) return;
        CVector at = heli->GetPosition();
        // What a rocket exploding within 8 m of it does (CHeli::TestRocketCollision); nothing while the script keeps it explosion proof.
        if (CHeli::TestRocketCollision(&at)) Log::Write("Shot the helicopter in g" + std::to_string(heliGlobal) + " down, as a rocket would");
    };
}

bool Steps::NeverTired() {
    return CWorld::Players[CWorld::PlayerInFocus].m_bInfiniteSprint;
}

void Steps::GetCar() {
    if (!FindPlayerVehicle()) Actions::GiveCar();
}

void Steps::PickUpFriend() {
    if (!FindPlayerVehicle()) {
        Actions::GiveCar();
        return;
    }
    const tRadarTrace* blip = Actions::MissionBlip(false);
    CVector target;
    if (!blip || blip->m_nBlipType != BLIP_CHAR || !Actions::BlipPosition(*blip, target)) return;
    // The mission checks the player in a car, stopped within 8 m of her (LOCATE_PLAYER_IN_CAR_CHAR_2D 8 and IS_PLAYER_STOPPED).
    if (Actions::Distance2D(Actions::PlayerPosition(), target) >= 8.0f) Actions::TeleportBeside(target);
}

void Steps::PickUpNearestFriend() {
    if (!FindPlayerVehicle()) {
        Actions::GiveCar();
        return;
    }
    // A friend who joined walks to the car and gets in by herself: leaving now would leave her behind.
    if (Actions::GroupMemberOnFoot()) return;
    const tRadarTrace* blip = Actions::NearestMissionBlip(BLIP_CHAR);
    CVector target;
    if (!blip || !Actions::BlipPosition(*blip, target)) return;
    if (Actions::Distance2D(Actions::PlayerPosition(), target) >= 8.0f) Actions::TeleportBeside(target);
}

void Steps::GetTaxi() {
    if (!FindPlayerVehicle()) Actions::GiveCar(MODEL_TAXI);
}

Steps::Action Steps::GetCarOf(int model) {
    return [model] {
        if (FindPlayerVehicle()) {
            if (Command<Commands::IS_PLAYER_IN_MODEL>(CWorld::PlayerInFocus, model)) return;
            Actions::LeaveCarTo(Actions::PlayerPosition() + CVector(0.0f, 4.0f, 0.0f));
            return;
        }
        Actions::GiveCar(model);
    };
}

Steps::Action Steps::EnterScriptCar(int carGlobal) {
    return [carGlobal] {
        if (ScriptCar(carGlobal)) Actions::EnterCar(Actions::ScriptGlobal(carGlobal));
    };
}

void Steps::HuntMarked() {
    if (Actions::KillMarkedCharacters() > 0) return;
    const tRadarTrace* blip = Actions::NearestMissionBlip(BLIP_COORD);
    CVector target;
    if (blip && Actions::BlipPosition(*blip, target) && Actions::Distance2D(Actions::PlayerPosition(), target) > 2.0f) Actions::TeleportTo(target);
}

Steps::Action Steps::ThrowGrenadeInto(CVector place) {
    return [place] { Actions::ThrowGrenade(place); };
}

Steps::Action Steps::ActivateTimedBomb(int carGlobal) {
    return [carGlobal] {
        CVehicle* car = ScriptCar(carGlobal);
        if (!car || Command<Commands::IS_CAR_ARMED_WITH_BOMB>(Actions::ScriptGlobal(carGlobal), BOMB_TIMED_ACTIVATED)) return;
        Command<Commands::ARM_CAR_WITH_BOMB>(Actions::ScriptGlobal(carGlobal), BOMB_TIMED_ACTIVATED);
        car->m_nBombTimer = 7000;
        Log::Write("Activated the timed bomb of the car in g" + std::to_string(carGlobal) + ", 7 s fuse");
    };
}

void Steps::KillMarkedCarOccupants() {
    Actions::KillMarkedCarOccupants();
}

Steps::Action Steps::KillScriptChars(std::vector<int> charGlobals) {
    return [charGlobals = std::move(charGlobals)] {
        int killed = 0;
        for (int global : charGlobals) {
            const int handle = Actions::ScriptGlobal(global);
            if (!CPools::GetPed(handle) || Command<Commands::IS_CHAR_DEAD>(handle)) continue;
            Command<Commands::EXPLODE_CHAR_HEAD>(handle);
            ++killed;
        }
        if (killed > 0) Log::Write("Killed " + std::to_string(killed) + " character(s) of the mission");
    };
}

Steps::Action Steps::KillScriptCharsWith(std::vector<int> charGlobals, eWeaponType weapon) {
    return [charGlobals = std::move(charGlobals), weapon] {
        int killed = 0;
        for (int global : charGlobals) {
            const int handle = Actions::ScriptGlobal(global);
            CPed* ped = CPools::GetPed(handle);
            if (!ped || Command<Commands::IS_CHAR_DEAD>(handle)) continue;
            // Dealt by the player with this weapon, which HAS_CHAR_BEEN_DAMAGED_BY_WEAPON then sees (the Rumble's bats only).
            ped->InflictDamage(FindPlayerPed(), weapon, 1000.0f, PEDPIECE_TORSO, 0);
            ++killed;
        }
        if (killed > 0) Log::Write("Killed " + std::to_string(killed) + " character(s) of the mission with weapon " + std::to_string(static_cast<int>(weapon)));
    };
}

Steps::Action Steps::ExplodeScriptCar(int carGlobal) {
    return [carGlobal] {
        const int handle = Actions::ScriptGlobal(carGlobal);
        if (!ScriptCar(carGlobal) || Command<Commands::IS_CAR_DEAD>(handle)) return;
        Command<Commands::EXPLODE_CAR>(handle);
        Log::Write("Blew up the car in g" + std::to_string(carGlobal));
    };
}

Steps::Action Steps::BoardScriptCar(int carGlobal) {
    return [carGlobal] {
        CVehicle* car = ScriptCar(carGlobal);
        if (!car || FindPlayerVehicle() == car) return;
        // Straight in: beside a boat is the water.
        Command<Commands::WARP_PLAYER_INTO_CAR>(CWorld::PlayerInFocus, Actions::ScriptGlobal(carGlobal));
        Log::Write("Boarded the vehicle in g" + std::to_string(carGlobal));
    };
}

Steps::Action Steps::SailTo(CVector place, std::vector<int> charGlobals) {
    return [place, charGlobals = std::move(charGlobals)] {
        if (!FindPlayerVehicle() || Actions::Distance2D(Actions::PlayerPosition(), place) <= 3.0f) return;
        std::vector<int> handles;
        for (int global : charGlobals) handles.push_back(Actions::ScriptGlobal(global));
        Actions::TeleportCarrying(place, handles);
    };
}

Steps::Action Steps::EnsureMoney(int amount) {
    return [amount] {
        if (Command<Commands::IS_SCORE_GREATER>(CWorld::PlayerInFocus, amount - 1)) return;
        Command<Commands::ADD_SCORE>(CWorld::PlayerInFocus, amount);
        Log::Write("Gave the player $" + std::to_string(amount));
    };
}

Steps::Action Steps::ClockToScriptTime(int hourGlobal, int minuteGlobal, int minutesBefore) {
    return [hourGlobal, minuteGlobal, minutesBefore] {
        const int hour = Actions::ScriptGlobal(hourGlobal), minute = Actions::ScriptGlobal(minuteGlobal);
        int minutesLeft = 0;
        Command<Commands::GET_MINUTES_TO_TIME_OF_DAY>(hour, minute, &minutesLeft);
        if (minutesLeft <= minutesBefore + 1) return;
        const int target = ((hour * 60 + minute - minutesBefore) % 1440 + 1440) % 1440;
        Command<Commands::SET_TIME_OF_DAY>(target / 60, target % 60);
        Log::Write("Moved the clock to " + std::to_string(target / 60) + ":" + std::to_string(target % 60) + ", " + std::to_string(minutesBefore) + " minutes before " + std::to_string(hour) + ":" + std::to_string(minute));
    };
}

Steps::Action Steps::GoBesideScriptChar(int charGlobal, float distance) {
    return [charGlobal, distance] {
        const int handle = Actions::ScriptGlobal(charGlobal);
        CPed* ped = CPools::GetPed(handle);
        if (!ped || Command<Commands::IS_CHAR_DEAD>(handle)) return;
        if (Actions::Distance2D(Actions::PlayerPosition(), ped->GetPosition()) > distance + 0.5f) Actions::TeleportBeside(ped->GetPosition(), distance);
    };
}

Steps::Action Steps::ProofScriptCar(int carGlobal) {
    return [carGlobal, proofed = -1]() mutable {
        const int handle = Actions::ScriptGlobal(carGlobal);
        if (handle == proofed || !ScriptCar(carGlobal)) return;
        Command<Commands::SET_CAR_PROOFS>(handle, 1, 1, 1, 1, 1);
        Log::Write("Made the car in g" + std::to_string(carGlobal) + " proof against bullets, fire, explosions, collisions and melee");
        proofed = handle;
    };
}

Steps::Action Steps::StayNearScriptCar(int carGlobal) {
    return [carGlobal] {
        CVehicle* car = ScriptCar(carGlobal);
        if (car && FindPlayerVehicle() != car && Actions::Distance2D(Actions::PlayerPosition(), car->GetPosition()) > 15.0f) Actions::TeleportBeside(car->GetPosition());
    };
}

Steps::Action Steps::DriveScriptCarAlong(int carGlobal, int phaseGlobal, std::vector<RoutePoint> route) {
    return [carGlobal, phaseGlobal, route = std::move(route), handle = -1, next = std::size_t{0}]() mutable {
        CVehicle* car = ScriptCar(carGlobal);
        if (!car) return;
        // A replayed mission creates a new car, which starts the route again; the car drives off by itself between two actions, so the point reached is kept here.
        if (Actions::ScriptGlobal(carGlobal) != handle) {
            handle = Actions::ScriptGlobal(carGlobal);
            next = 0;
        }
        // The script must see the car at each point before it sends it on: a point ahead of its phase waits.
        if (next >= route.size() || route[next].phase > Actions::ScriptGlobal(phaseGlobal)) return;
        const CVector& place = route[next].place;
        Command<Commands::LOAD_SCENE>(place.x, place.y, place.z);
        Command<Commands::SET_CAR_COORDINATES>(handle, place.x, place.y, place.z);
        Log::Write("Moved the car in g" + std::to_string(carGlobal) + " to route point " + std::to_string(next + 1) + "/" + std::to_string(route.size()));
        ++next;
    };
}

void Steps::SailToNearestBlip() {
    if (!FindPlayerVehicle()) return;
    const tRadarTrace* blip = Actions::NearestMissionBlip(BLIP_OBJECT);
    if (!blip) blip = Actions::NearestMissionBlip(BLIP_COORD);
    CVector target;
    if (!blip || !Actions::BlipPosition(*blip, target) || Actions::Distance2D(Actions::PlayerPosition(), target) <= 3.0f) return;
    Actions::TeleportCarrying(target, {});
}

void Steps::SailToFloatingPackages() {
    if (!FindPlayerVehicle()) return;
    const CVector from = Actions::PlayerPosition();
    CObject* nearest = nullptr;
    for (const CPickup& pickup : CPickups::aPickUps) {
        if (pickup.m_bRemoved || !pickup.m_pObject || (pickup.m_nPickupType != PICKUP_FLOATINGPACKAGE && pickup.m_nPickupType != PICKUP_FLOATINGPACKAGE_FLOATING)) continue;
        if (!nearest || Actions::Distance2D(pickup.m_pObject->GetPosition(), from) < Actions::Distance2D(nearest->GetPosition(), from)) nearest = pickup.m_pObject;
    }
    if (!nearest) {
        SailToNearestBlip();
        return;
    }
    // The game collects a package once it floats and touches the boat (CPickup::Update, a 2 m sphere).
    if (Actions::Distance2D(nearest->GetPosition(), from) > 1.0f) Actions::TeleportCarrying(nearest->GetPosition(), {});
}

Steps::Action Steps::SmashMarkedObjectsNear(float distance) {
    return [distance] {
        const CVector at = Actions::PlayerPosition();
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_OBJECT) continue;
            CObject* object = CPools::GetObject(blip.m_nEntityHandle);
            if (!object || Actions::Distance2D(object->GetPosition(), at) > distance) continue;
            // The damage a car driven into it deals: the object breaks, which HAS_OBJECT_BEEN_DAMAGED sees (bRenderDamaged, or no longer visible).
            object->ObjectDamage(1000.0f);
            Log::Write("Smashed a marked object");
        }
    };
}

void Steps::ExplodeMarkedObjects() {
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        CVector position;
        if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_OBJECT || !Actions::BlipPosition(blip, position)) continue;
        Command<Commands::ADD_EXPLOSION>(position.x, position.y, position.z, 0);
        Log::Write("Set off an explosion on a marked object");
    }
}

void Steps::GetOutAside() {
    if (FindPlayerVehicle()) Actions::LeaveCarTo(Actions::PlayerPosition() + CVector(0.0f, 8.0f, 0.0f));
}

void Steps::FollowRadar() {
    if (Actions::KillMarkedCharacters() > 0) return;
    if (Actions::NearestMissionBlip(BLIP_OBJECT)) {
        CollectPickup();
        return;
    }
    if (const tRadarTrace* car = Actions::NearestMissionBlip(BLIP_CAR)) {
        Actions::EnterCar(car->m_nEntityHandle);
        return;
    }
    GoToBlip();
}

Steps::Action Steps::FollowRadarSparing(std::vector<int> charGlobals) {
    return [charGlobals = std::move(charGlobals)] {
        int killed = 0;
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_CHAR || !CPools::GetPed(blip.m_nEntityHandle)) continue;
            bool spared = false;
            for (int global : charGlobals) spared = spared || Actions::ScriptGlobal(global) == blip.m_nEntityHandle;
            if (spared || Command<Commands::IS_CHAR_DEAD>(blip.m_nEntityHandle)) continue;
            Command<Commands::EXPLODE_CHAR_HEAD>(blip.m_nEntityHandle);
            ++killed;
        }
        if (killed > 0) {
            Log::Write("Killed " + std::to_string(killed) + " marked enemy(ies)");
            return;
        }
        if (Actions::NearestMissionBlip(BLIP_OBJECT)) {
            CollectPickup();
            return;
        }
        CVector target;
        if (const tRadarTrace* car = Actions::NearestMissionBlip(BLIP_CAR); car && Actions::BlipPosition(*car, target)) {
            if (Actions::Distance2D(Actions::PlayerPosition(), target) > 15.0f) Actions::TeleportBeside(target);
            return;
        }
        GoToBlip();
    };
}

void Steps::KeepAlive() {
    CPlayerPed* player = FindPlayerPed();
    player->m_fHealth = 100.0f;
    player->m_fArmour = 100.0f;
    if (CVehicle* car = FindPlayerVehicle()) car->m_fHealth = 1000.0f;
}

Steps::Action Steps::LureMarkedCarsTo(CVector trap) {
    return [trap] {
        bool moved = false;
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_CAR) continue;
            CVehicle* car = CPools::GetVehicle(blip.m_nEntityHandle);
            if (!car || Command<Commands::IS_CAR_DEAD>(blip.m_nEntityHandle) || Actions::Distance2D(car->GetPosition(), trap) < 10.0f) continue;
            Command<Commands::LOAD_SCENE>(trap.x, trap.y, trap.z);
            Command<Commands::SET_CAR_COORDINATES>(blip.m_nEntityHandle, trap.x, trap.y, trap.z);
            Log::Write("Moved a marked car into the trap");
            moved = true;
        }
        if (moved || Actions::NearestMissionBlip(BLIP_CAR)) return;
        // The nearest marked place that is not the trap itself, where the next car shows up.
        const tRadarTrace* nearest = nullptr;
        float nearestDistance = 0.0f;
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            CVector position;
            if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_COORD || !Actions::BlipPosition(blip, position) || Actions::Distance2D(position, trap) < 60.0f) continue;
            const float distance = Actions::Distance2D(Actions::PlayerPosition(), position);
            if (!nearest || distance < nearestDistance) {
                nearest = &blip;
                nearestDistance = distance;
            }
        }
        CVector target;
        if (nearest && Actions::BlipPosition(*nearest, target) && nearestDistance > 5.0f) Actions::TeleportTo(target);
    };
}

Steps::Action Steps::TourPlaces(std::vector<CVector> places) {
    return [places = std::move(places)] {
        if (places.empty()) return;
        std::size_t next = 0;
        for (std::size_t i = 0; i < places.size(); ++i) {
            if (Actions::Distance2D(Actions::PlayerPosition(), places[i]) < 10.0f) next = (i + 1) % places.size();
        }
        Actions::TeleportTo(places[next]);
    };
}

namespace {
    // Kills the living characters of these models within 120 m of the player, each first counted as the player's kill with this weapon, as the game counts one; returns how many.
    int KillModelsAsPlayer(const std::vector<int>& models, eWeaponType weapon) {
        CPlayerPed* player = FindPlayerPed();
        auto* pool = CPools::ms_pPedPool;
        int killed = 0;
        for (int i = 0; i < pool->m_nSize; ++i) {
            CPed* ped = pool->GetAt(i);
            if (!ped || ped == player || Actions::Distance2D(ped->GetPosition(), Actions::PlayerPosition()) > 120.0f) continue;
            const int handle = CPools::GetPedRef(ped);
            if (Command<Commands::IS_CHAR_DEAD>(handle)) continue;
            bool wanted = false;
            for (int model : models) wanted = wanted || (model >= 0 && ped->m_nModelIndex == model);
            if (!wanted) continue;
            // Counted first (kill frenzies, GET_NUM_OF_MODELS_KILLED_BY_PLAYER), as the game does; EXPLODE_CHAR_HEAD alone has no killer.
            CDarkel::RegisterKillByPlayer(ped, weapon, false);
            Command<Commands::EXPLODE_CHAR_HEAD>(handle);
            ++killed;
        }
        return killed;
    }
}

void Steps::KillFrenzyTargets() {
    if (!CDarkel::FrenzyOnGoing()) return;
    const int killed = KillModelsAsPlayer({CDarkel::ModelToKill, CDarkel::ModelToKill2, CDarkel::ModelToKill3, CDarkel::ModelToKill4}, CDarkel::WeaponType);
    if (killed > 0) Log::Write("Counted and killed " + std::to_string(killed) + " frenzy target(s)");
}

Steps::Action Steps::BlowUpCarsOfModel(int model) {
    return [model] {
        const CVector at = Actions::PlayerPosition();
        auto* pool = CPools::ms_pVehiclePool;
        int blown = 0;
        for (int i = 0; i < pool->m_nSize; ++i) {
            CVehicle* car = pool->GetAt(i);
            if (!car || car == FindPlayerVehicle() || car->m_nModelIndex != model || Actions::Distance2D(car->GetPosition(), at) > 150.0f) continue;
            const int handle = CPools::GetVehicleRef(car);
            if (Command<Commands::IS_CAR_DEAD>(handle)) continue;
            // Counted first, as the game counts a car the player blows up (GET_NUM_OF_MODELS_KILLED_BY_PLAYER); EXPLODE_CAR alone has no culprit.
            CDarkel::RegisterCarBlownUpByPlayer(car);
            Command<Commands::EXPLODE_CAR>(handle);
            ++blown;
        }
        if (blown > 0) {
            Log::Write("Counted and blew up " + std::to_string(blown) + " car(s) of model " + std::to_string(model));
            return;
        }
        // Gang cars are rare in traffic: one is created 25 m off, far enough that its explosion spares the player's own vehicle.
        Command<Commands::REQUEST_MODEL>(model);
        Command<Commands::LOAD_ALL_MODELS_NOW>();
        int car = 0;
        Command<Commands::CREATE_CAR>(model, at.x + 25.0f, at.y, -100.0f, &car);
        Command<Commands::MARK_CAR_AS_NO_LONGER_NEEDED>(car);
        Command<Commands::MARK_MODEL_AS_NO_LONGER_NEEDED>(model);
    };
}

Steps::Action Steps::KillModelsNear(std::vector<int> models) {
    return [models = std::move(models)] {
        const int killed = KillModelsAsPlayer(models, WEAPONTYPE_UZI);
        if (killed > 0) Log::Write("Counted and killed " + std::to_string(killed) + " character(s) of the wanted models");
    };
}

void Steps::ExtinguishMarkedCars() {
    int put = 0;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_CAR) continue;
        CVehicle* car = CPools::GetVehicle(blip.m_nEntityHandle);
        if (!car) continue;
        // What the fire truck's water cannon does to the fire on it, which IS_SCRIPT_FIRE_EXTINGUISHED then sees.
        gFireManager.ExtinguishPoint(car->GetPosition(), 6.0f);
        ++put;
    }
    if (put > 0) Log::Write("Put out the fire on " + std::to_string(put) + " marked car(s)");
}

Steps::Action Steps::FerryMarked(std::vector<CVector> streets, bool newCarWhenStuck) {
    return [streets = std::move(streets), newCarWhenStuck, fareHandle = -1, nearSince = 0, letGoInARow = 0]() mutable {
        CVehicle* car = FindPlayerVehicle();
        if (!car || streets.empty()) return;
        const int now = CTimer::m_snTimeInMilliseconds;
        const CVector at = Actions::PlayerPosition();
        const tRadarTrace* fare = Actions::NearestMissionBlip(BLIP_CHAR);
        const tRadarTrace* destination = Actions::NearestMissionBlip(BLIP_COORD);
        CVector place;
        // A marked place means someone got in.
        if (destination) letGoInARow = 0;
        // Pick up while there is room and someone is marked, else drive the passengers to the marked place.
        if (fare && car->m_nNumPassengers < car->m_nMaxPassengers && Actions::BlipPosition(*fare, place)) {
            if (fare->m_nEntityHandle != fareHandle) {
                fareHandle = fare->m_nEntityHandle;
                nearSince = 0;
            }
            // A fare gets in a stopped car within 7 m of them in 3D (LOCATE_PLAYER_IN_CAR_CHAR_3D 7, 7, 2): 6 m off, at their height, as a car 4 m off can hit them.
            if (Actions::Distance2D(at, place) > 6.5f || std::fabs(at.z - place.z) > 1.5f) {
                Actions::TeleportBeside(place, 6.0f);
                return;
            }
            if (!nearSince) nearSince = now;
            if (now - nearSince < 12000) return;
            // One who cannot reach the car is let go 90 m away (the mission then looks for another): off to the street farthest from them.
            const CVector* farthest = &streets[0];
            for (const CVector& street : streets) {
                if (Actions::Distance2D(street, place) > Actions::Distance2D(*farthest, place)) farthest = &street;
            }
            char line[160];
            std::snprintf(line, sizeof(line), "Leaving a fare who cannot reach the car: %.1f m away, %.1f m higher, %d passenger(s) aboard", Actions::Distance2D(at, place), place.z - at.z, static_cast<int>(car->m_nNumPassengers));
            Log::Write(line);
            nearSince = 0;
            if (newCarWhenStuck && ++letGoInARow >= 3) {
                Log::Write("Three fares in a row could not get in: getting out, which ends the job, to start it again in a new car");
                letGoInARow = 0;
                Actions::LeaveCarTo(at + CVector(0.0f, 6.0f, 0.0f));
                return;
            }
            Actions::TeleportTo(*farthest);
            return;
        }
        if (destination && Actions::BlipPosition(*destination, place)) {
            if (Actions::Distance2D(at, place) > 2.0f) StopAt(place)();
            return;
        }
        // Nobody marked: a fare only hails a moving car among passers-by, so the streets are toured.
        std::size_t next = 0;
        for (std::size_t i = 0; i < streets.size(); ++i) {
            if (Actions::Distance2D(at, streets[i]) < 10.0f) next = (i + 1) % streets.size();
        }
        Actions::TeleportTo(streets[next]);
    };
}

void Steps::TouchCoordBlips() {
    const CVector at = Actions::PlayerPosition();
    const tRadarTrace* nearest = nullptr;
    CVector target;
    float best = 1e9f;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        CVector place;
        if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_COORD || !Actions::BlipPosition(blip, place)) continue;
        // In 3D, for checkpoints stacked on the floors of a car park (Multistorey Mayhem).
        const float distance = (place - at).Magnitude();
        if (distance < 1.5f || distance >= best) continue;
        best = distance;
        nearest = &blip;
        target = place;
    }
    if (nearest) Actions::PlaceAt(target + CVector(0.0f, 0.0f, 1.0f));
}

void Steps::AlternateCoordBlips() {
    const tRadarTrace* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        CVector position;
        if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_COORD || !Actions::BlipPosition(blip, position)) continue;
        const float distance = Actions::Distance2D(Actions::PlayerPosition(), position);
        if (distance > 6.0f && (!nearest || distance < nearestDistance)) {
            nearest = &blip;
            nearestDistance = distance;
        }
    }
    CVector target;
    if (nearest && Actions::BlipPosition(*nearest, target)) Actions::TeleportTo(target);
}

Steps::Action Steps::MoveScriptCarTo(int carGlobal, CVector place) {
    return [carGlobal, place] {
        CVehicle* car = ScriptCar(carGlobal);
        if (!car || Actions::Distance2D(car->GetPosition(), place) < 3.0f) return;
        Command<Commands::SET_CAR_COORDINATES>(Actions::ScriptGlobal(carGlobal), place.x, place.y, place.z);
        // A teleported car can stay static, hanging where it was put: woken and nudged down, it falls (into the sea, which IS_CAR_IN_WATER then sees).
        car->bIsStatic = false;
        car->m_vecMoveSpeed = CVector(0.0f, 0.0f, -0.1f);
        Log::Write("Moved the car in g" + std::to_string(carGlobal));
    };
}

Steps::Action Steps::DestroyMarkedCarsExcept(std::vector<int> carGlobals) {
    return [carGlobals = std::move(carGlobals)] {
        for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
            if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_CAR || !CPools::GetVehicle(blip.m_nEntityHandle)) continue;
            bool kept = false;
            for (int global : carGlobals) kept = kept || Actions::ScriptGlobal(global) == blip.m_nEntityHandle;
            if (kept || Command<Commands::IS_CAR_DEAD>(blip.m_nEntityHandle)) continue;
            Command<Commands::EXPLODE_CAR>(blip.m_nEntityHandle);
            Log::Write("Blew up a marked car");
        }
    };
}

Steps::Action Steps::DriveScriptCar(int carGlobal, Action then) {
    return [carGlobal, then = std::move(then)] {
        CVehicle* car = ScriptCar(carGlobal);
        if (!car) return;
        if (FindPlayerVehicle() != car) Actions::EnterCar(Actions::ScriptGlobal(carGlobal));
        else then();
    };
}

Steps::Action Steps::CollectByCarAndDeliver(int carriedGlobal, int batch, CVector dropOff, CVector field) {
    return [carriedGlobal, batch, dropOff, field] {
        if (!FindPlayerVehicle()) {
            Actions::GiveCar();
            return;
        }
        const int carried = Actions::ScriptGlobal(carriedGlobal);
        const tRadarTrace* blip = Actions::NearestMissionBlip(BLIP_OBJECT);
        CVector target;
        bool found = blip && Actions::BlipPosition(*blip, target);
        // Unmarked, like the platinum of Bullion Run (START_PACMAN_SCRAMBLE): the nearest bar left on the field, which a car collects by touching it.
        float nearest = 1e9f;
        for (const CPacManPickup& bar : CPacManPickups::aPickUps) {
            if (blip || bar.m_nType == 0 || Actions::Distance2D(bar.m_vecPos, field) > 150.0f) continue;
            const float distance = Actions::Distance2D(bar.m_vecPos, Actions::PlayerPosition());
            if (distance >= nearest) continue;
            nearest = distance;
            target = bar.m_vecPos;
            found = true;
        }
        if (carried < batch && found) Actions::TeleportTo(target);
        else if (carried > 0 && Actions::Distance2D(Actions::PlayerPosition(), dropOff) > 2.0f) Actions::TeleportTo(dropOff);
        else if (!found && Actions::Distance2D(Actions::PlayerPosition(), field) > 10.0f) Actions::TeleportTo(field);
    };
}

void Steps::CollectPickup() {
    // The nearest pickup blip, whatever places or characters the radar also marks (Shima marks the gang's spots too).
    const tRadarTrace* blip = Actions::NearestMissionBlip(BLIP_OBJECT);
    CVector target;
    if (!blip || !Actions::BlipPosition(*blip, target)) return;
    WalkTo(target)();
}

void Steps::KillMarked() {
    Actions::KillMarkedCharacters();
}

void Steps::DestroyMarkedCars() {
    Actions::ExplodeMarkedCars();
}

void Steps::WreckMarkedCars() {
    int damaged = 0;
    for (const tRadarTrace& blip : CRadar::ms_RadarTrace) {
        if (!blip.m_bInUse || blip.m_nRadarSprite != 0 || blip.m_nBlipType != BLIP_CAR) continue;
        CVehicle* car = CPools::GetVehicle(blip.m_nEntityHandle);
        if (!car || car->m_fHealth <= 950.0f) continue;
        car->m_fHealth = 900.0f;
        ++damaged;
    }
    if (damaged) {
        Log::Write("Damaged " + std::to_string(damaged) + " marked car(s) first, as the player's shots would");
        return;
    }
    Actions::KillMarkedCarOccupants();
    Actions::ExplodeMarkedCars();
}

void Steps::Honk() {
    Actions::Honk();
}

void Steps::ClearWanted() {
    if (Command<Commands::IS_WANTED_LEVEL_GREATER>(CWorld::PlayerInFocus, 0)) {
        Command<Commands::CLEAR_WANTED_LEVEL>(CWorld::PlayerInFocus);
        Log::Write("Cleared the wanted level");
    }
}

Steps::Action Steps::SetClock(int hour) {
    return [hour] { Command<Commands::SET_TIME_OF_DAY>(hour, 0); };
}

Steps::Action Steps::ArmBomb(int carGlobal, int bombType) {
    return [carGlobal, bombType] {
        if (!ScriptCar(carGlobal) || Command<Commands::IS_CAR_ARMED_WITH_BOMB>(Actions::ScriptGlobal(carGlobal), bombType)) return;
        Command<Commands::ARM_CAR_WITH_BOMB>(Actions::ScriptGlobal(carGlobal), bombType);
        Log::Write("Fitted a bomb of type " + std::to_string(bombType) + " to the car in g" + std::to_string(carGlobal));
    };
}

Steps::Action Steps::SetCarHealth(int carGlobal, int health) {
    return [carGlobal, health] {
        if (!ScriptCar(carGlobal)) return;
        Command<Commands::SET_CAR_HEALTH>(Actions::ScriptGlobal(carGlobal), health);
        Log::Write("Set the health of the car in g" + std::to_string(carGlobal) + " to " + std::to_string(health));
    };
}

Steps::Action Steps::ParkAt(CVector place, float heading) {
    return [place, heading] {
        // Teleporting again would not let the car settle where the game counts it.
        if (!FindPlayerVehicle() || Actions::Distance2D(Actions::PlayerPosition(), place) <= 1.0f) return;
        Actions::TeleportTo(place);
        Actions::SetCarHeading(heading);
        // The camera stays where it looked before the teleport until told otherwise, and some missions wait for the player to look at something.
        Command<Commands::SET_CAMERA_BEHIND_PLAYER>();
    };
}

Steps::Action Steps::LogPointOnScreen(CVector point) {
    return [point, last = -1]() mutable {
        const int onScreen = Command<Commands::IS_POINT_ON_SCREEN>(point.x, point.y, point.z, 4.0f) ? 1 : 0;
        if (onScreen == last) return;
        Log::Write(std::string("The point ") + std::to_string(point.x) + ", " + std::to_string(point.y) + (onScreen ? " is on screen" : " is off screen"));
        last = onScreen;
    };
}

Steps::Action Steps::RollInto(CVector start, float heading, float speed) {
    return [start, heading, speed] {
        if (!FindPlayerVehicle()) return;
        // Once it has rolled on from the start it is in, or settling: taking it back out would fail the mission.
        const float rolled = Actions::Distance2D(Actions::PlayerPosition(), start);
        if (rolled > 4.0f && rolled < 15.0f) return;
        Actions::TeleportTo(start);
        Actions::SetCarHeading(heading);
        Command<Commands::SET_CAMERA_BEHIND_PLAYER>();
        // Driven in, the car lands on a floor the ground search does not see (a garage's), where a car teleported inside falls through.
        Actions::PushCar(speed, 1800);
    };
}

Steps::Action Steps::GetOutTo(CVector place) {
    return [place] {
        if (FindPlayerVehicle()) Actions::LeaveCarTo(place);
    };
}
