#include <cstring>
#include <initializer_list>
#include <string>
#include "plugin.h"
#include "CTheScripts.h"
#include "Log.h"
#include "MainScript.h"
#include "Options.h"
#include "Story.h"

namespace {
    using MainScript::Command;

    // The globals of the locks, by the byte offset in ScriptSpace main.scm numbers them with: the passed flags of Salvatore's Called a Meeting and Chaperone, and Salvatore's lock.
    constexpr unsigned short kMeetingPassed = 1052;
    constexpr unsigned short kChaperonePassed = 1068;
    constexpr unsigned short kSalvatoreLock = 1088;

    // The pause: the triggers of Pump-Action Pimp, The Fuzz Ball, Dead Skunk in the Trunk and The Getaway end themselves while the meeting is passed and Chaperone is not, until Chaperone's pass starts them again.
    const std::string kPauseTest = Command(COMMAND_IS_INT_VAR_EQUAL_TO_NUMBER, MainScript::Global(kMeetingPassed) + MainScript::Int8(1)) +
                                   Command(COMMAND_IS_INT_VAR_EQUAL_TO_NUMBER, MainScript::Global(kChaperonePassed) + MainScript::Int8(0)) +
                                   Command(COMMAND_GOTO_IF_FALSE, {MainScript::kInt32});

    // Turns the jump after each pause test into a plain GOTO, in the game's copy of main.scm: the pause is always skipped; CTheScripts::Init loads main.scm again for every game, and a save only keeps the variables.
    void ReleasePause() {
        const std::string& scm = MainScript::Bytes();
        int released = 0;
        for (std::size_t at = scm.find(kPauseTest); at != std::string::npos && at < MainScript::MainSize(); at = scm.find(kPauseTest, at + 1)) {
            unsigned char* code = CTheScripts::ScriptSpace + at;
            if (std::memcmp(code, kPauseTest.data(), kPauseTest.size()) != 0) continue;
            // The jump's opcode is the pattern's last 2 bytes before the argument's type byte; GOTO takes the same argument.
            unsigned char* jump = code + kPauseTest.size() - 3;
            jump[0] = COMMAND_GOTO & 0xFF;
            jump[1] = COMMAND_GOTO >> 8;
            ++released;
        }
        Log::Write("Story: released the pause between Salvatore's Called a Meeting and Chaperone in " + std::to_string(released) + " triggers");
    }

    // Salvatore's triggers only start while his lock is 0; Cutting the Grass and Bomb Da Base: Act I set it, Blow Fish and Bomb Da Base: Act II clear it, so in another order it may never clear: it is kept at 0.
    void KeepSalvatoreFree() {
        int& lock = *reinterpret_cast<int*>(CTheScripts::ScriptSpace + kSalvatoreLock);
        if (!lock) return;
        lock = 0;
        Log::Write("Story: cleared Salvatore's lock (g1088), set by Cutting the Grass or Bomb Da Base: Act I");
    }

    // The flags that say a fee is paid: Bomb Da Base: Act II only starts once 8-Ball has $100000 or is paid (g24), The Exchange's trigger once Catalina has $500000 or is paid (g1684), and each takes the money only while unpaid.
    constexpr unsigned short kEightBallPaid = 24;
    constexpr unsigned short kCatalinaPaid = 1684;

    // need_money off: both are paid already, so neither mission asks for its money nor takes it; main.scm clears the flags at a new game's start, so they are set again on every frame.
    void WaiveFees() {
        if (Options::NeedMoney()) return;
        for (const unsigned short paid : {kEightBallPaid, kCatalinaPaid}) *reinterpret_cast<int*>(CTheScripts::ScriptSpace + paid) = 1;
    }

    // Runs once per frame, after the scripts.
    void Process() {
        KeepSalvatoreFree();
        WaiveFees();
    }
}

void Story::Install() {
    // initScriptsEvent follows CTheScripts::Init, which a new game, a loaded one and a restart all run, once main.scm is loaded.
    plugin::Events::initScriptsEvent += ReleasePause;
    plugin::Events::gameProcessEvent += Process;
}
