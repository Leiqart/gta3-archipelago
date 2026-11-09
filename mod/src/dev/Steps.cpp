#include <cmath>
#include <utility>
#include "plugin.h"
#include "CDarkel.h"
#include "CGarage.h"
#include "CGarages.h"
#include "CPhoneInfo.h"
#include "CPickups.h"
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

void Steps::KillFrenzyTargets() {
    if (!CDarkel::FrenzyOnGoing()) return;
    const int models[] = {CDarkel::ModelToKill, CDarkel::ModelToKill2, CDarkel::ModelToKill3, CDarkel::ModelToKill4};
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
        // Counted first, as the game does when the player kills one with the frenzy's weapon; EXPLODE_CHAR_HEAD alone has no killer.
        CDarkel::RegisterKillByPlayer(ped, CDarkel::WeaponType, false);
        Command<Commands::EXPLODE_CHAR_HEAD>(handle);
        ++killed;
    }
    if (killed > 0) Log::Write("Counted and killed " + std::to_string(killed) + " frenzy target(s)");
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

Steps::Action Steps::CollectByCarAndDeliver(int carriedGlobal, int batch, CVector dropOff) {
    return [carriedGlobal, batch, dropOff] {
        if (!FindPlayerVehicle()) {
            Actions::GiveCar();
            return;
        }
        const tRadarTrace* pickup = Actions::NearestMissionBlip(BLIP_OBJECT);
        const int carried = Actions::ScriptGlobal(carriedGlobal);
        CVector target;
        if (carried < batch && pickup && Actions::BlipPosition(*pickup, target)) Actions::TeleportTo(target);
        else if (carried > 0 && Actions::Distance2D(Actions::PlayerPosition(), dropOff) > 2.0f) Actions::TeleportTo(dropOff);
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

Steps::Action Steps::GetOutTo(CVector place) {
    return [place] {
        if (FindPlayerVehicle()) Actions::LeaveCarTo(place);
    };
}
