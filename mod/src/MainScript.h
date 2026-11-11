#pragma once
#include <string>
#include <vector>
#include "eScriptCommands.h"

// The game's own data/main.scm, read once on first use: the main part, which the game loads at ScriptSpace[0], then each mission's code.
namespace MainScript {
    // The type byte main.scm writes before an argument's value: a 32-bit number, a global variable, an 8-bit number, a coordinate (16 bits, in sixteenths).
    constexpr char kInt32 = 1, kGlobal = 2, kInt8 = 4, kCoordinate = 6;
    // A command as main.scm stores it, to search the file for: its opcode in 2 bytes, low byte first, then its arguments.
    std::string Command(eScriptCommands opcode, const std::string& arguments = "");
    // A global variable argument, by the byte offset in ScriptSpace main.scm numbers it with (g528 is 528).
    std::string Global(unsigned short offset);
    // An 8-bit number argument.
    std::string Int8(signed char value);
    // The whole file; empty if it could not be read.
    const std::string& Bytes();
    // The size of the main part: positions below it are the same in the file and in ScriptSpace.
    std::size_t MainSize();
    // Each mission's code by mission number, as LOAD_AND_LAUNCH_MISSION_INTERNAL numbers them; empty for one the header points outside the file.
    const std::vector<std::string>& Missions();
    // True when a text key starts at this position: 1 to 7 of A-Z, 0-9 or _, then a zero byte.
    bool IsKeyAt(const std::string& code, std::size_t at);
    // A mission's name from its code: its first SCRIPT_NAME (opcode 0x03A4 and the name), in lowercase as the game stores it ("luigi2"); empty if it has none.
    std::string ScriptName(const std::string& code);
}
