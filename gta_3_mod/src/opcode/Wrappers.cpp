#include "Wrappers.h"
#include "../MissionTable.h"
#include <cstring>

bool Opcode::SwitchRoadsOn(Executor& executor, const Bounds& bounds) {
    return executor.Execute({Id::SwitchRoadsOn, {
        bounds.first.x, bounds.first.y, bounds.first.z,
        bounds.second.x, bounds.second.y, bounds.second.z}});
}

bool Opcode::SwapNearestBuildingModel(Executor& executor, Point center, float radius, int fromModel, int toModel) {
    return executor.Execute({Id::SwapNearestBuildingModel, {
        center.x, center.y, center.z, radius, fromModel, toModel}});
}

bool Opcode::SetClosestObjectVisibility(Executor& executor, Point center, float radius, int model, bool visible) {
    return executor.Execute({Id::SetClosestObjectVisibility, {
        center.x, center.y, center.z, radius, model, static_cast<int>(visible)}});
}

bool Opcode::ClearPassage(Executor& executor, Point center, float radius, int model) {
    // Bound the loop even if a broken backend keeps returning the same handle.
    for (int attempt = 0; attempt < 8; ++attempt) {
        int object = -1;
        if (!executor.Execute({Id::GetClosestObjectOfType, {
                center.x, center.y, center.z, radius, model, &object}})) return false;
        if (object == -1) return true; // The area may not have been streamed yet.
        bool exists = false;
        if (!executor.Execute({Id::DoesObjectExist, {object}}, &exists)) return false;
        if (!exists) return true;
        if (!executor.Execute({Id::SetObjectCollision, {object, 0}}) ||
            !SetClosestObjectVisibility(executor, center, radius, model, false) ||
            // GET_CLOSEST_OBJECT_OF_TYPE searches in 2D. Move X/Y too, so the
            // next lookup can find another barrier rather than the same handle.
            !executor.Execute({Id::SetObjectCoordinates, {object, 0.0f, 0.0f, -1000.0f}})) return false;
    }
    return false;
}

bool Opcode::SetIndustrialPassed(Executor& executor) { return executor.Execute({Id::IndustrialPassed, {}}); }
bool Opcode::SetCommercialPassed(Executor& executor) { return executor.Execute({Id::CommercialPassed, {}}); }

bool Opcode::AddMoney(Executor& executor, int player, int amount) {
    return executor.Execute({Id::AddScore, {player, amount}});
}

bool Opcode::AddArmor(Executor& executor, int player, int amount) {
    return executor.Execute({Id::AddArmourToPlayer, {player, amount}});
}

bool Opcode::GiveWeapon(Executor& executor, int player, int type, int ammo, int model) {
    if (!executor.Execute({Id::RequestModel, {model}})) return false;
    if (!executor.Execute({Id::LoadAllModelsNow, {}})) return false;
    bool loaded = false;
    if (!executor.Execute({Id::HasModelLoaded, {model}}, &loaded) || !loaded) return false;
    const bool given = executor.Execute({Id::GiveWeaponToPlayer, {player, type, ammo}});
    // Releasing a model is housekeeping; a successful grant must not be retried.
    executor.Execute({Id::ReleaseModel, {model}});
    return given;
}

bool Opcode::GetPlayerPosition(Executor& executor, int player, Point& position) {
    int x = 0, y = 0, z = 0;
    if (!executor.Execute({Id::GetPlayerCoordinates, {player, &x, &y, &z}})) return false;
    static_assert(sizeof(int) == sizeof(float));
    std::memcpy(&position.x, &x, sizeof(x));
    std::memcpy(&position.y, &y, sizeof(y));
    std::memcpy(&position.z, &z, sizeof(z));
    return true;
}

bool Opcode::PlayerOnFoot(Executor& executor, int player, bool& onFoot) {
    bool inCar = false;
    if (!executor.Execute({Id::IsPlayerInAnyCar, {player}}, &inCar)) return false;
    onFoot = !inCar;
    return true;
}

bool Opcode::PlayerCanStartMission(Executor& executor, int player, bool& canStart) {
    return executor.Execute({Id::CanPlayerStartMission, {player}}, &canStart);
}

bool Opcode::Fading(Executor& executor, bool& fading) { return executor.Execute({Id::IsFading, {}}, &fading); }

bool Opcode::AddCoordinateBlip(Executor& executor, Point position, int sprite, int& handle) {
    handle = -1;
    // Coordinate blips do not add an implicit sphere; Render owns the 3D marker.
    return executor.Execute({Id::AddCoordinateBlip, {position.x, position.y, position.z, sprite, &handle}}) && handle >= 0;
}

bool Opcode::HideBlip(Executor& executor, int handle) {
    return handle < 0 || executor.Execute({Id::ChangeBlipDisplay, {handle, 0}});
}

bool Opcode::RemoveBlip(Executor& executor, int handle) {
    return handle < 0 || executor.Execute({Id::RemoveBlip, {handle}});
}

bool Opcode::DrawMissionMarker(Executor& executor, Point position) {
    return executor.Execute({Id::DrawSphere, {position.x, position.y, position.z, 2.0f}});
}

bool Opcode::BeginMissionFade(Executor& executor, int player) {
    return executor.Execute({Id::MakePlayerSafe, {player}}) &&
        executor.Execute({Id::SetFadingColor, {0, 0, 0}}) && executor.Execute({Id::DoFade, {500, 0}});
}

bool Opcode::LaunchMission(Executor& executor, int mission) {
    const int native = MissionTable::NativeIndex(mission);
    return native >= 0 && executor.Execute({Id::LaunchMission, {native}});
}

bool Opcode::PrepareLuigisGirls(Executor& executor, int& playerPed, int& eightball) {
    bool loaded = false;
    playerPed = eightball = -1;
    if (!executor.Execute({Id::GetPlayerChar, {0, &playerPed}}) || playerPed < 0 ||
        !executor.Execute({Id::LoadSpecialCharacter, {1, std::string("EIGHT2")}}) ||
        !executor.Execute({Id::LoadAllModelsNow, {}}) ||
        !executor.Execute({Id::HasSpecialCharacterLoaded, {1}}, &loaded) || !loaded ||
        !executor.Execute({Id::LoadScene, {900.75f, -427.5f, 13.875f}})) return false;
    // Special slot 1 is model 26 in GTA III. Native LM1 deletes this actor
    // before creating its cutscene objects; it still needs a valid handle.
    if (!executor.Execute({Id::CreateChar, {21, 26, 897.25f, -424.5625f, 13.875f, &eightball}}) || eightball < 0)
        return false;
    if (executor.Execute({Id::SetCharAnimationGroup, {eightball, 9}}) &&
        executor.Execute({Id::SetCharHeading, {eightball, 90.0f}})) return true;
    executor.Execute({Id::DeleteChar, {eightball}});
    eightball = -1;
    return false;
}

bool Opcode::ReleaseMissionCar(Executor& executor, int handle) {
    return handle < 0 || executor.Execute({Id::ReleaseCar, {handle}});
}

bool Opcode::RestoreFreeRoam(Executor& executor, int player) {
    // Run every restoration even if one command fails; never strand controls.
    bool restored = true;
    const Command commands[] = {
        {Id::MakePlayerUnsafe, {}}, {Id::CancelOverrideRestart, {}},
        {Id::SwitchStreaming, {1}}, {Id::SetPlayerControl, {player, 1}},
        {Id::SwitchWidescreen, {0}}, {Id::SetPoliceIgnorePlayer, {player, 0}},
        {Id::SetEveryoneIgnorePlayer, {player, 0}}, {Id::RestoreCameraJumpcut, {}},
        {Id::SetCameraBehindPlayer, {}}, {Id::DoFade, {500, 1}}};
    for (const auto& command : commands) if (!executor.Execute(command)) restored = false;
    return restored;
}
