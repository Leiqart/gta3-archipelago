#include "IntroSequence.h"
#include <fstream>
#include <iterator>

bool IntroSequence::SupportedScript(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != 606553) return false;
    std::uint64_t hash = 14695981039346656037ull;
    for (auto byte : bytes) hash = (hash ^ byte) * 1099511628211ull;
    // Original GTA III main.scm: SHA256 E4F809E770418842565C86D133EDF88F
    // 42C85D61A4825D1322CD70C584D17E96. No game script is distributed.
    return hash == 0x2e6ea9aff5694e54ull;
}

bool IntroSequence::SupportedScript(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::file_size(path, error) != 606553 || error) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    return SupportedScript(std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), {}));
}

void IntroSequence::Controller::Begin(int logicalMission) {
    Reset();
    if (logicalMission == kGiveMeLiberty || logicalMission == kLuigisGirls) mission_ = logicalMission;
}

void IntroSequence::Controller::Reset() {
    mission_ = -1;
    prepared_ = finishing_ = false;
}

IntroSequence::Action IntroSequence::Controller::BeforeCommand(std::uint32_t ip, std::uint16_t opcode) const {
    if (!Active()) return Action::None;
    if (ip == kInitialize && opcode == 0x0004) return Action::Initialize;
    if (mission_ == kLuigisGirls && !prepared_ && ip == kDirectEntry && opcode == 0x00D6)
        return Action::PrepareLuigi;
    if (mission_ == kGiveMeLiberty && !finishing_ && ip == kHideoutFinished && opcode == 0x016E)
        return Action::FinishGml;
    // A failed GML keeps the vanilla critical restart; a failed LG returns to its marker instead of replaying GML.
    if (mission_ == kLuigisGirls && opcode == 0x0255) return Action::SkipCriticalRestart;
    return Action::None;
}
