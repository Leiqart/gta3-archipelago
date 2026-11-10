#pragma once
#include <string>
#include <vector>
#include "CPlaceable.h"
#include "eModelID.h"
#include "CRadar.h"
#include "CVector.h"

// Dev builds only: what the cheat keys and the autopilot can do in the game; call it on the game thread only.
namespace Actions {
    // Shows a short line on screen; the game keeps the pointer, so only pass string literals.
    void Say(const wchar_t* text);
    // The mission's current objective on the radar: a place first, then a character, a car, and pickups only if asked; nullptr when there is none.
    const tRadarTrace* MissionBlip(bool withPickups);
    // Where a blip points: its own position for a place, else the position of the character, car or pickup it follows.
    bool BlipPosition(const tRadarTrace& blip, CVector& position);
    // Where the player is, or their car; read from the player itself, because the game's FindPlayerCoors crashed once right after a cutscene.
    CVector PlayerPosition();
    // Distance on the map, ignoring height.
    float Distance2D(const CVector& a, const CVector& b);
    // Loads the area, then puts the player on the ground there; the game moves the car and its passengers too.
    void TeleportTo(const CVector& place);
    // Puts the player at a place's own height, without looking for the ground, for a marker under a low roof the ground search would land on.
    void PlaceAt(const CVector& place);
    // Teleports the player (and their car) this far beside a character or car, on a side whose ground is at its height.
    void TeleportBeside(const CVector& entity, float distance = 4.0f);
    // Teleports to the mission blip, beside it for a character, car or pickup; false when there is no blip.
    bool GoToMissionBlip();
    // Kills every living character the mission marks with a blip; returns how many.
    int KillMarkedCharacters();
    // Blows up every car the mission marks with a blip and that is not wrecked yet; returns how many.
    int ExplodeMarkedCars();
    // Kills the driver and passengers of every car the mission marks; returns how many.
    int KillMarkedCarOccupants();
    // The mission blip of this type nearest to the player, pickups left out; nullptr when there is none.
    const tRadarTrace* NearestMissionBlip(eBlipType type);
    // True when any mission blip, pickups and objects included, lies within this distance of a place on the map.
    bool MissionBlipNear(const CVector& place, float distance);
    // Drops a live grenade from the player at a place, as if thrown there: for the missions that check a projectile in a box (Silence the Sneak).
    void ThrowGrenade(const CVector& place);
    // True while a character who follows the player (a friend the mission made join the player's group) is still on foot.
    bool GroupMemberOnFoot();
    // Gives the player a car of this model and seats them in it; by default a Kuruma, a 4-door car every mission accepts (Drive Misty For Me refuses vans and trucks).
    void GiveCar(int model = MODEL_KURUMA);
    // Teleports the player's car (a boat) to a place on the map at its own height, carrying these characters with it at the same offsets, as if they stood on its deck.
    void TeleportCarrying(const CVector& place, const std::vector<int>& charHandles);
    // Seats the player at the wheel of a car: a player in another car is first taken out, and one more than 10 m away first goes beside it.
    void EnterCar(int carHandle);
    // Writes the player's position and heading to the log, or their car's and its speed when they are in one (F9, and when the autopilot is stuck).
    void LogWhereIAm();
    // Writes a place and its heading in degrees, the unit SET_CAR_HEADING takes: 0 faces north, 90 west.
    void LogPlaceAndHeading(const std::string& what, CPlaceable& placeable);
    // Turns the player's car to a heading in degrees (0 faces north, 90 west), as SET_CAR_HEADING does.
    void SetCarHeading(float degrees);
    // Holds the horn button for one second, through a hook on CPad::GetHorn, the function the car and IS_PLAYER_PRESSING_HORN ask.
    void Honk();
    // Drives the player's car straight ahead at this speed (m/s) for this long, kept up every frame by UpdatePush as a held accelerator would.
    void PushCar(float speed, int milliseconds);
    // Keeps up the push PushCar started; called once per frame.
    void UpdatePush();
    // Gives the player back control, the normal camera and streaming, as a mission does; used after a mission is forced to pass from its start.
    void GiveControlBack();
    // Changes Claude's outfit as a mission does: UNDRESS_CHAR, LOAD_ALL_MODELS_NOW, DRESS_CHAR; "PLAYER" is his normal one, "PLAYERP" the prison one.
    void DressPlayer(const char* model);
    // Takes the player out of their car and puts them on foot at a place.
    void LeaveCarTo(const CVector& place);
    // The SCRIPT_NAME of the mission running now, in lowercase like the game stores it ("eight"); nullptr between missions.
    const char* RunningMission();
    // Reads a global variable of the game's main script; offset is the number after "g" in the decoded script (g980 is 980).
    int ScriptGlobal(int offset);
    // Writes a global variable of the game's main script, like a mission's countdown.
    void SetScriptGlobal(int offset, int value);
}
