#pragma once
// PlayerRuntime: small shared helpers for one-shot SCM bytecode stubs.

namespace PlayerRuntime {
    namespace ScmSlots {
        constexpr std::uint32_t FromEnd(std::uint32_t bytesFromEnd) {
            return static_cast<std::uint32_t>(ScriptRuntime::kSizeMainScript) - bytesFromEnd;
        }

        constexpr std::uint32_t SafeStateOffset            = FromEnd(16);
        constexpr std::uint32_t CancelRestartOffset        = FromEnd(32);
        constexpr std::uint32_t PrintNowOffset             = FromEnd(64);
        constexpr std::uint32_t WantedOffset               = FromEnd(96);
        constexpr std::uint32_t SpawnCarOffset             = FromEnd(160);
        constexpr std::uint32_t VehicleTeleportOffset      = FromEnd(256);
        constexpr std::uint32_t MissionVehiclePrepOffset   = FromEnd(384);
        constexpr std::uint32_t AddScoreOffset             = FromEnd(448);
        constexpr std::uint32_t PrepareIntroModeOffset     = FromEnd(512);
        constexpr std::uint32_t LoadCollisionOffset        = FromEnd(576);
        constexpr std::uint32_t ClearContactBlipsOffset    = FromEnd(704);
        constexpr std::uint32_t RestorePlayerOutfitOffset  = FromEnd(768);
        constexpr std::uint32_t GiveWeaponsOffset          = FromEnd(896);
        constexpr std::uint32_t ContactBlipSyncOffset      = FromEnd(1024);
        constexpr std::uint32_t ServiceBlipOffset          = FromEnd(1152);
        constexpr std::uint32_t SetCurrentWeaponOffset     = FromEnd(1280);
        constexpr std::uint32_t GmlFailFastRestoreOffset   = FromEnd(1344);
        constexpr std::uint32_t FreeRoamRestoreOffset      = FromEnd(1472);
        constexpr std::uint32_t GmlFreeRoamRestoreOffset   = FromEnd(1600);
        constexpr std::uint32_t VanillaPrelaunchOffset     = FromEnd(1664);

        constexpr std::size_t SafeStateBytes          = 16;
        constexpr std::size_t CancelRestartBytes      = 16;
        constexpr std::size_t PrintNowBytes           = 32;
        constexpr std::size_t WantedBytes             = 16;
        constexpr std::size_t SpawnCarBytes           = 64;
        constexpr std::size_t VehicleTeleportBytes    = 96;
        constexpr std::size_t MissionVehiclePrepBytes = 128;
        constexpr std::size_t AddScoreBytes           = 64;
        constexpr std::size_t PrepareIntroModeBytes   = 16;
        constexpr std::size_t ClearContactBlipsBytes  = 128;
        constexpr std::size_t RestorePlayerOutfitBytes = 64;
        constexpr std::size_t GiveWeaponsBytes        = 128;
        constexpr std::size_t ContactBlipSyncBytes    = 128;
        constexpr std::size_t ServiceBlipBytes        = 128;
        constexpr std::size_t SetCurrentWeaponBytes   = 64;
        constexpr std::size_t GmlFailFastRestoreBytes = 32;
        constexpr std::size_t FreeRoamRestoreBytes    = 64;
        constexpr std::size_t GmlFreeRoamRestoreBytes = 96;
        constexpr std::size_t VanillaPrelaunchBytes   = 64;
    }

    inline CRunningScript* StartHelperScript(std::uint32_t ip) {
        // Helper SCM stubs run below kSizeMainScript, so the StartNewScript hook
        // never routes them through the mission-slot blocker. They must not
        // overwrite the authorization latch reserved for the real mission launch.
        return ScriptRuntime::StartNewScript()(ip);
    }

    inline CRunningScript* LaunchScmStub(std::uint32_t ip,
                                         const std::uint8_t* bytes,
                                         std::size_t byteCount,
                                         std::size_t slotBytes,
                                         const char* tag) {
        if (!ScriptRuntime::HasLiveScriptEngine() || !bytes ||
            byteCount == 0 || byteCount > slotBytes) {
            if (tag) {
                Logger::Log("%s: stub launch skipped (ip=%u size=%zu slot=%zu)",
                            tag, ip, byteCount, slotBytes);
            }
            return nullptr;
        }

        std::uint8_t* dest = ScriptRuntime::ScriptSpace() + ip;
        std::memcpy(dest, bytes, byteCount);
        if (slotBytes > byteCount) {
            std::memset(dest + byteCount, 0, slotBytes - byteCount);
        }
        CRunningScript* s = StartHelperScript(ip);
        if (!s && tag) {
            Logger::Log("%s: helper script queue failed (ip=%u)", tag, ip);
        }
        return s;
    }

    template <std::size_t N>
    inline CRunningScript* LaunchScmStub(std::uint32_t ip,
                                         const std::uint8_t (&bytes)[N],
                                         std::size_t slotBytes,
                                         const char* tag) {
        return LaunchScmStub(ip, bytes, N, slotBytes, tag);
    }
}
