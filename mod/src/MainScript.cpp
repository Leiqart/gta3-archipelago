#include <windows.h>
#include <cctype>
#include <fstream>
#include <iterator>
#include "Log.h"
#include "MainScript.h"

namespace {
    // The file and what its header says.
    struct File {
        std::string bytes;
        std::size_t mainSize = 0;
        std::vector<std::string> missions;
    };

    // The 4-byte number at this position; 0 past the end of the file.
    unsigned int Read32(const std::string& bytes, std::size_t at) {
        return at + 4 <= bytes.size() ? *reinterpret_cast<const unsigned int*>(bytes.data() + at) : 0u;
    }

    // Reads data/main.scm next to gta3.exe and its header, laid out as openrw's SCMFile reads it.
    File Load() {
        File file;
        char exe[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        std::string folder = exe;
        folder = folder.substr(0, folder.find_last_of('\\') + 1);
        std::ifstream stream(folder + "data\\main.scm", std::ios::binary);
        file.bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        // The file starts with a GOTO (2 bytes of opcode, 1 of type, 4 of target) to the models segment, whose own GOTO leads to the missions segment.
        const std::size_t segment = Read32(file.bytes, Read32(file.bytes, 3) + 3);
        // The missions segment: its GOTO and a byte, then the main part's size, the largest mission's size, the number of missions, and where each one starts.
        if (segment + 20 > file.bytes.size()) {
            Log::Write("Could not read the header of data\\main.scm");
            return file;
        }
        file.mainSize = Read32(file.bytes, segment + 8);
        const unsigned int count = Read32(file.bytes, segment + 16);
        for (unsigned int i = 0; i < count; ++i) {
            const std::size_t start = Read32(file.bytes, segment + 20 + 4 * i);
            const std::size_t end = i + 1 < count ? Read32(file.bytes, segment + 24 + 4 * i) : file.bytes.size();
            file.missions.push_back(start < end && end <= file.bytes.size() ? file.bytes.substr(start, end - start) : std::string());
        }
        Log::Write("Read data\\main.scm: a main part of " + std::to_string(file.mainSize) + " bytes and " + std::to_string(file.missions.size()) + " missions");
        return file;
    }

    // The file, read on first use.
    const File& TheFile() {
        static const File file = Load();
        return file;
    }
}

const std::string& MainScript::Bytes() { return TheFile().bytes; }
std::size_t MainScript::MainSize() { return TheFile().mainSize; }
const std::vector<std::string>& MainScript::Missions() { return TheFile().missions; }

std::string MainScript::ScriptName(const std::string& code) {
    for (std::size_t at = code.find("\xA4\x03", 0, 2); at != std::string::npos; at = code.find("\xA4\x03", at + 1, 2)) {
        if (!IsKeyAt(code, at + 2)) continue;
        std::string name;
        for (std::size_t i = at + 2; code[i]; ++i) name += static_cast<char>(std::tolower(static_cast<unsigned char>(code[i])));
        return name;
    }
    return "";
}

bool MainScript::IsKeyAt(const std::string& code, std::size_t at) {
    for (std::size_t length = 0; length < 8 && at + length < code.size(); ++length) {
        const char c = code[at + length];
        if (c == 0) return length > 0;
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return false;
}
