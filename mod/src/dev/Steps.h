#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "CRadar.h"
#include "CVector.h"

// Dev builds only: the dictionary of goals and actions the mission sheets are written with (Sheets.cpp); everything runs on the game thread.
namespace Steps {
    // A test of the game: true once the mission has reached the goal of a step.
    using Goal = std::function<bool()>;
    // What the autopilot does until the goal is reached, again every 2 s; each action looks at the game first, so repeating it is harmless.
    using Action = std::function<void()>;

    // One step of a mission sheet: the autopilot repeats its action until its goal is reached, then goes on to the next step.
    struct Step {
        // Written to the log, like "luigi2 step 3/6: respray the car at the Pay 'n' Spray".
        std::string name;
        // UntilMissionEnds for the last step of a sheet: the mission registers its pass and ends in the same frame.
        Goal goal;
        // JustWait when the mission moves on by itself.
        Action action;
        // Seconds the step may take, scripted scenes included, before the autopilot counts as stuck.
        int timeoutSeconds = 30;
    };

    // A step without a goal lasts until the mission ends.
    constexpr std::nullptr_t UntilMissionEnds = nullptr;
    // A step without an action only waits for the mission.
    constexpr std::nullptr_t JustWait = nullptr;

    // Goal: the player is in a car.
    bool InCar();
    // Goal: the player is on foot.
    bool OnFoot();
    // Goal: the radar shows no mission blip, pickups left out.
    bool NoBlip();
    // Goal: the mission blip is of this type (BLIP_CAR, BLIP_CHAR or BLIP_COORD).
    Goal BlipIs(eBlipType type);
    // Goal: the mission blip is within 20 m of a place on the map, so the mission has moved its objective there.
    Goal BlipNear(CVector place);
    // Goal: the player, or their car, is within this distance of a place on the map.
    Goal PlayerNear(CVector place, float distance);
    // Goal: the door of the mission garage whose index the script keeps in this global is open.
    Goal GarageOpen(int garageGlobal);
    // Goal: the player's car is entirely inside the mission garage whose index the script keeps in this global.
    Goal CarInGarage(int garageGlobal);
    // Goal: the car the script keeps in this global carries this type of bomb, as the mission's own IS_CAR_ARMED_WITH_BOMB sees it.
    Goal CarArmedWith(int carGlobal, int bombType);
    // Goal: the car the script keeps in this global has this health or less (1000 is new, 250 burns).
    Goal CarHealthAtMost(int carGlobal, int health);
    // Goal: the car the script keeps in this global is stopped inside a box, as the mission's own IS_CAR_STOPPED_IN_AREA_3D sees it.
    Goal CarStoppedIn(int carGlobal, CVector corner, CVector oppositeCorner);
    // Goal: nobody sits in the car the script keeps in this global, neither at the wheel nor as a passenger.
    Goal CarEmpty(int carGlobal);
    // Goal: the character the script keeps in this global is out of every car.
    Goal CharOnFoot(int charGlobal);
    // Goal: the player's car carries at least this many passengers.
    Goal PassengersAtLeast(int count);
    // Goal: a counter the script keeps in this global has reached this value, like the girls dropped at the Fuzz Ball (g6844).
    Goal GlobalAtLeast(int global, int value);
    // Goal: both goals are reached.
    Goal AllOf(Goal first, Goal second);
    // Goal: any mission blip, pickups and objects (like a ringing phone) included, lies within 20 m of a place on the map.
    Goal AnyBlipNear(CVector place);
    // Goal: the radar marks at least one mission blip of this type, whatever the others.
    Goal Marked(eBlipType type);
    // Goal: the player sits in a car of this model (IS_PLAYER_IN_MODEL).
    Goal InModel(int model);
    // Goal: the player sits in the car the script keeps in this global.
    Goal InScriptCar(int carGlobal);

    // Action: does two actions, one after the other.
    Action Both(Action first, Action second);
    // Action: gets in the car the mission marks, going beside it first when it is far.
    void EnterBlipCar();
    // Action: teleports to the mission blip, beside it for a character or a car; nothing once there.
    void GoToBlip();
    // Action: teleports to a place; nothing once within 2 m of it.
    Action GoTo(CVector place);
    // Action: teleports into an area a mission checks the player stopped in (within 1 m of its centre), and clears the wanted level these missions refuse.
    Action StopAt(CVector place);
    // Action: puts the player on foot at a place, out of their car first, for the markers a mission checks on foot; nothing once there.
    Action WalkTo(CVector place);
    // Action: gives the player a Kuruma when on foot.
    void GetCar();
    // Action: picks up the friend the mission marks, like Misty: in a car, within 8 m of her, and she gets in by herself.
    void PickUpFriend();
    // Action: like PickUpFriend for the nearest marked character, ignoring places on the radar; waits while a friend who joined still walks to the car.
    void PickUpNearestFriend();
    // Action: gives the player a taxi when on foot, the only cars some characters get into (Curly Bob).
    void GetTaxi();
    // Action: gives the player a car of this model when they are not in one already, like the police car or Yardie car a mission asks to steal.
    Action GetCarOf(int model);
    // Action: gets in the car the script keeps in this global, going beside it first when it is far.
    Action EnterScriptCar(int carGlobal);
    // Action: kills the marked characters, or, when none is marked, goes to the nearest place the radar marks, where the next one shows up (Smack Down).
    void HuntMarked();
    // Action: drops a live grenade at a place, for a mission that wants a projectile thrown in there.
    Action ThrowGrenadeInto(CVector place);
    // Action: activates a timed bomb on the car the script keeps in this global, with the 7 s fuse the game gives when the player arms it.
    Action ActivateTimedBomb(int carGlobal);
    // Action: walks onto the pickup the mission marks, which collects it.
    void CollectPickup();
    // Action: kills the driver and passengers of every car the mission marks.
    void KillMarkedCarOccupants();
    // Action: kills the characters the script keeps in these globals, those still alive.
    Action KillScriptChars(std::vector<int> charGlobals);
    // Action: blows up the car the script keeps in this global.
    Action ExplodeScriptCar(int carGlobal);
    // Action: seats the player at the wheel of the car (or boat) the script keeps in this global, from wherever they are.
    Action BoardScriptCar(int carGlobal);
    // Action: teleports the player's boat to a place, carrying the characters the script keeps in these globals on its deck; nothing once within 3 m.
    Action SailTo(CVector place, std::vector<int> charGlobals);
    // Action: gives the player this much money when they have less, as a mission that charges it requires.
    Action EnsureMoney(int amount);
    // Action: moves the clock to a few minutes before the hour and minute the script keeps in these globals, when it is still further away.
    Action ClockToScriptTime(int hourGlobal, int minuteGlobal, int minutesBefore);
    // Action: kills every character the mission marks.
    void KillMarked();
    // Action: blows up every car the mission marks.
    void DestroyMarkedCars();
    // Action: holds the horn for one second.
    void Honk();
    // Action: clears the player's wanted level.
    void ClearWanted();
    // Action: sets the game clock to this hour, for the missions that only start, or only pass, at some hours.
    Action SetClock(int hour);
    // Action: fits a bomb to the car the script keeps in this global, as 8-Ball's garage or the player's fire button does (ARM_CAR_WITH_BOMB).
    Action ArmBomb(int carGlobal, int bombType);
    // Action: sets the health of the car the script keeps in this global, like the damage a player would deal.
    Action SetCarHealth(int carGlobal, int health);
    // Action: parks the player's car at a place and heading; once there, leaves it alone so the game can count it in.
    Action ParkAt(CVector place, float heading);
    // Action: takes the player out of their car to a place on foot; nothing once on foot.
    Action GetOutTo(CVector place);
}
