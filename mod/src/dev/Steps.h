#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "CRadar.h"
#include "CVector.h"
#include "eWeaponType.h"

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
    // Goal: the other goal is not reached, like no pickup left on the radar.
    Goal Not(Goal goal);
    // Goal: any mission blip, pickups and objects (like a ringing phone) included, lies within 20 m of a place on the map.
    Goal AnyBlipNear(CVector place);
    // Goal: the radar marks at least one mission blip of this type, whatever the others.
    Goal Marked(eBlipType type);
    // Goal: the player sits in a car of this model (IS_PLAYER_IN_MODEL).
    Goal InModel(int model);
    // Goal: the player sits in the car the script keeps in this global.
    Goal InScriptCar(int carGlobal);
    // Goal: a kill frenzy (START_KILL_FRENZY) is on, like the drive-by of Uzi Money.
    bool Frenzy();
    // Goal: one of the characters the script keeps in these globals, still alive, is within this distance of the car the script keeps in that global.
    Goal AnyCharNear(std::vector<int> charGlobals, int carGlobal, float distance);

    // Action: does two actions, one after the other.
    Action Both(Action first, Action second);
    // Action: does the first action while the goal is reached, the other one otherwise.
    Action Either(Goal when, Action then, Action otherwise);
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
    // Action: stands in front of the pay phone nearest to a place, so the game lifts it when it rings; nothing once it is lifted.
    Action AnswerPhone(CVector place);
    // Action: writes these script globals to the log whenever one of them has changed, for studying a mission.
    Action LogGlobals(std::vector<int> globals);
    // Action: turns the player on foot to this heading (0 is north, 90 west), the camera behind them, for a mission that only brings enemies in off screen.
    Action FaceHeading(float degrees);
    // Action: puts the player on foot at a place's own height, for a marker under a low roof (the Panlantic lift); nothing once there.
    Action StandAt(CVector place);
    // Action: writes to the log whether a point is on screen whenever that changes, for a mission that waits for the player to look at something.
    Action LogPointOnScreen(CVector point);
    // Action: moves a mission's countdown in this global on to this many milliseconds when it shows more, like the clock moved on for a mission that waits for an hour.
    Action ShortenTimer(int timerGlobal, int milliseconds);
    // Action: shoots down the drug-run plane (START_DRUG_RUN) once it flies within this distance of a place, as a rocket fired at it would.
    Action ShootDownDrugPlane(CVector place, float distance);
    // Action: shoots down the helicopter the script keeps in this global, as a rocket fired at it would, once the script no longer keeps it explosion proof.
    Action ShootDownScriptHeli(int heliGlobal);
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
    // Action: teleports the player (and their car) this far beside the character the script keeps in this global, still alive, like a hostage the player must come close to.
    Action GoBesideScriptChar(int charGlobal, float distance);
    // Action: makes the car the script keeps in this global proof against everything, once, for a mission whose enemies are only there to wreck it (Decoy).
    Action ProofScriptCar(int carGlobal);
    // Action: teleports the player (and their car) beside the car the script keeps in this global whenever it is more than 15 m away.
    Action StayNearScriptCar(int carGlobal);
    // One point of a route a mission drives a car along, and the script's phase from which it sends the car there.
    struct RoutePoint {
        int phase;
        CVector place;
    };
    // Action: moves the car the script keeps in this global to the next point of a route, each once the phase the script keeps in that global allows it; nothing after the last.
    Action DriveScriptCarAlong(int carGlobal, int phaseGlobal, std::vector<RoutePoint> route);
    // Action: takes the player's boat onto the nearest marked pickup, else onto the nearest marked place, at the boat's own height.
    void SailToNearestBlip();
    // Action: takes the player's boat onto the nearest floating package, a pickup without a blip that a plane drops (A Drop in the Ocean), else onto the nearest marked place.
    void SailToFloatingPackages();
    // Action: smashes every object the mission marks within this distance of the player, without an explosion, like the espresso stalls a car is driven into.
    Action SmashMarkedObjectsNear(float distance);
    // Action: sets off an explosion on every object the mission marks, for what a mission wants smashed (the witness's bodycast).
    void ExplodeMarkedObjects();
    // Action: takes the player out of their car to a spot 8 m beside it.
    void GetOutAside();
    // Action: for missions read only in part: kills the marked characters, else collects the nearest pickup, else gets in the marked car, else goes to the marked place.
    void FollowRadar();
    // Action: like FollowRadar for a mission with friends on the radar: spares the characters the script keeps in these globals, and goes beside marked cars instead of in.
    Action FollowRadarSparing(std::vector<int> charGlobals);
    // Action: heals the player and repairs their car, for a mission that only asks to survive a while with the police on them (Decoy), or gunmen (Smack Down).
    void KeepAlive();
    // Action: moves every marked car, still whole, into a trap; with none marked, goes to the nearest marked place away from the trap, where the next one shows up (Bait).
    Action LureMarkedCarsTo(CVector trap);
    // Action: teleports the player to the place after the one they are near, round and round, for a mission whose targets only show up nearby (Espresso-2-Go).
    Action TourPlaces(std::vector<CVector> places);
    // Action: during a kill frenzy, kills the nearby characters of the models it wants, each one first counted as the player's kill with its weapon.
    void KillFrenzyTargets();
    // Action: blows up the nearby cars of this model, each counted as blown up by the player, or creates one 25 m off when none is near, for RC Toyz.
    Action BlowUpCarsOfModel(int model);
    // Action: kills the nearby characters of these models, each first counted as the player's Uzi kill, for a mission that counts the player's kills by model (GET_NUM_OF_MODELS_KILLED_BY_PLAYER).
    Action KillModelsNear(std::vector<int> models);
    // Action: puts the player (and their car) at the nearest marked place in 3D, at its own height, for checkpoint challenges whose places are all marked at once (the Patriot challenges, Multistorey Mayhem).
    void TouchCoordBlips();
    // Action: teleports to the nearest marked place more than 6 m away; with a race's current and next checkpoint both marked, it alternates and passes each.
    void AlternateCoordBlips();
    // Action: moves the car the script keeps in this global to a place once, like an empty car pushed into the sea.
    Action MoveScriptCarTo(int carGlobal, CVector place);
    // Action: blows up every marked car except those the script keeps in these globals (the player's own toy van).
    Action DestroyMarkedCarsExcept(std::vector<int> carGlobals);
    // Action: seats the player in the car the script keeps in this global first, then does the other action, for a mission that wants that car somewhere.
    Action DriveScriptCar(int carGlobal, Action then);
    // Action: drives onto the nearest marked pickup, or any pickup left on a field when none is marked, until the script counts this many carried (in this global), then drives to a drop-off.
    Action CollectByCarAndDeliver(int carriedGlobal, int batch, CVector dropOff, CVector field);
    // Action: walks onto the pickup the mission marks, which collects it.
    void CollectPickup();
    // Action: kills the driver and passengers of every car the mission marks.
    void KillMarkedCarOccupants();
    // Action: kills the characters the script keeps in these globals, those still alive.
    Action KillScriptChars(std::vector<int> charGlobals);
    // Action: kills the characters the script keeps in these globals, still alive, as the player's blows with this weapon, for a mission that checks the weapon used.
    Action KillScriptCharsWith(std::vector<int> charGlobals, eWeaponType weapon);
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
    // Action: damages every marked car first, as the player's shots would, for a mission that reacts to a car hit while still whole; then kills their occupants and blows them up.
    void WreckMarkedCars();
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
    // Action: puts the player's car at a start facing this heading and drives it ahead at this speed (m/s) for 1.8 s, again every step until the goal, for a garage whose floor a teleport falls through.
    Action RollInto(CVector start, float heading, float speed);
    // Action: takes the player out of their car to a place on foot; nothing once on foot.
    Action GetOutTo(CVector place);
}
