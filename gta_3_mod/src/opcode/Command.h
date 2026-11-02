#pragma once

#include <cstdint>
#include <variant>
#include <vector>
#include <string>

namespace Opcode {
    // Verified against the GTA III command reference; IDs stay in this layer.
    enum class Id : std::uint16_t {
        GetPlayerCoordinates = 0x0054,
        SetPlayerCoordinates = 0x0055,
        CreateChar = 0x009A,
        DeleteChar = 0x009B,
        IsPlayerInAnyCar = 0x00E0,
        AddScore = 0x0109,
        RemoveBlip = 0x0164,
        SetFadingColor = 0x0169,
        DoFade = 0x016A,
        IsFading = 0x016B,
        SetCharHeading = 0x0173,
        ChangeBlipDisplay = 0x018B,
        GiveWeaponToPlayer = 0x01B1,
        SetPlayerControl = 0x01B4,
        SetObjectCoordinates = 0x01BC,
        ReleaseCar = 0x01C3,
        SwitchRoadsOn = 0x01E7,
        CancelOverrideRestart = 0x01F6,
        GetPlayerChar = 0x01F5,
        SetPoliceIgnorePlayer = 0x01F7,
        LoadSpecialCharacter = 0x023C,
        HasSpecialCharacterLoaded = 0x023D,
        SetCharAnimationGroup = 0x0245,
        RequestModel = 0x0247,
        HasModelLoaded = 0x0248,
        ReleaseModel = 0x0249,
        SwitchWidescreen = 0x02A3,
        AddContactBlip = 0x02A7,
        AddCoordinateBlip = 0x02A8,
        RestoreCameraJumpcut = 0x02EB,
        IndustrialPassed = 0x034A,
        CommercialPassed = 0x034B,
        AddArmourToPlayer = 0x035E,
        SetClosestObjectVisibility = 0x0363,
        SetCameraBehindPlayer = 0x0373,
        SetObjectCollision = 0x0382,
        LoadAllModelsNow = 0x038B,
        DrawSphere = 0x03A1,
        SwitchStreaming = 0x03AF,
        SwapNearestBuildingModel = 0x03B6,
        SetEveryoneIgnorePlayer = 0x03BF,
        DoesObjectExist = 0x03CA,
        LoadScene = 0x03CB,
        CanPlayerStartMission = 0x03EE,
        MakePlayerSafe = 0x03EF,
        MakePlayerUnsafe = 0x03F6,
        LaunchMission = 0x0417,
        GetClosestObjectOfType = 0x045D,
    };

    // Output pointers become local SCM variables; never expose game globals.
    using Parameter = std::variant<int, float, int*, std::string>;
    struct Command {
        Id id;
        std::vector<Parameter> parameters;
    };

    // Throws on unrepresentable GTA III fixed-point floats or invalid outputs.
    std::vector<std::uint8_t> Encode(const Command& command);

    class Executor {
    public:
        virtual ~Executor() = default;
        // True means dispatched. Conditional opcodes return their result separately.
        virtual bool Execute(const Command& command, bool* condition = nullptr) = 0;
    };
}
