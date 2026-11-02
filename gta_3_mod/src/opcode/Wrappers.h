#pragma once
#include "Command.h"

namespace Opcode {
    struct Point { float x, y, z; };
    struct Bounds { Point first, second; };

    bool SwitchRoadsOn(Executor& executor, const Bounds& bounds);
    bool SwapNearestBuildingModel(Executor& executor, Point center, float radius, int fromModel, int toModel);
    bool SetClosestObjectVisibility(Executor& executor, Point center, float radius, int model, bool visible);
    // Disable and move barriers below ground while retaining vanilla script handles.
    bool ClearPassage(Executor& executor, Point center, float radius, int model);
    bool SetIndustrialPassed(Executor& executor);
    bool SetCommercialPassed(Executor& executor);
    bool AddMoney(Executor& executor, int player, int amount);
    bool AddArmor(Executor& executor, int player, int amount);
    bool GiveWeapon(Executor& executor, int player, int type, int ammo, int model);
    bool GetPlayerPosition(Executor& executor, int player, Point& position);
    bool PlayerOnFoot(Executor& executor, int player, bool& onFoot);
    bool PlayerCanStartMission(Executor& executor, int player, bool& canStart);
    bool Fading(Executor& executor, bool& fading);
    bool AddCoordinateBlip(Executor& executor, Point position, int sprite, int& handle);
    bool HideBlip(Executor& executor, int handle);
    bool RemoveBlip(Executor& executor, int handle);
    bool DrawMissionMarker(Executor& executor, Point position);
    bool BeginMissionFade(Executor& executor, int player);
    bool LaunchMission(Executor& executor, int mission);
    bool PrepareLuigisGirls(Executor& executor, int& playerPed, int& eightball);
    bool ReleaseMissionCar(Executor& executor, int handle);
    bool RestoreFreeRoam(Executor& executor, int player);
}
