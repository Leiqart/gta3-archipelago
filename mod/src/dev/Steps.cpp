#include <utility>
#include "plugin.h"
#include "CGarage.h"
#include "CGarages.h"
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

Steps::Action Steps::Both(Action first, Action second) {
    return [first = std::move(first), second = std::move(second)] {
        first();
        second();
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
    };
}

Steps::Action Steps::GetOutTo(CVector place) {
    return [place] {
        if (FindPlayerVehicle()) Actions::LeaveCarTo(place);
    };
}
