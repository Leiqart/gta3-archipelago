#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace IntroSequence {
    constexpr int kGiveMeLiberty = 1000; // logical mission; shares native EIGHT with Luigi's Girls
    constexpr int kLuigisGirls = 21;
    constexpr std::uint32_t kMissionBase = 128 * 1024;
    constexpr std::uint32_t kInitialize = 46;
    constexpr std::uint32_t kDirectEntry = 304;
    constexpr std::uint32_t kHideoutFinished = 8717;
    constexpr std::uint32_t kLuigiCutscene = 11106;
    constexpr std::uint32_t kCleanupReturn = 17;

    // Vanilla owns EIGHT for the intro bootstrap and every critical restart until Give Me Liberty is checked.
    constexpr bool VanillaOwnsEight(bool bootstrapSeen, bool gmlChecked) { return !bootstrapSeen || !gmlChecked; }

    // Exact vanilla layout required before any mission redirection is enabled.
    bool SupportedScript(const std::vector<std::uint8_t>& bytes);
    bool SupportedScript(const std::filesystem::path& path);

    enum class Action { None, Initialize, PrepareLuigi, FinishGml, SkipCriticalRestart };
    class Controller {
    public:
        void Begin(int logicalMission);
        void Reset();
        Action BeforeCommand(std::uint32_t relativeIp, std::uint16_t opcode) const;
        void Prepared() { prepared_ = true; }
        void FinishedGml() { finishing_ = true; }
        bool Active() const { return mission_ != -1; }
        int Mission() const { return mission_; }
    private:
        int mission_ = -1;
        bool prepared_ = false, finishing_ = false;
    };
}
