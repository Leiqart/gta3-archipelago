#include <sstream>
#include <string>
#include "ClientLink.h"
#include "Log.h"
#include "Options.h"

namespace {
    bool g_needMoney = true;

    // One option: its name in the YAML and its value as a number.
    void Set(const std::string& name, int value) {
        if (name == "need_money") {
            if (g_needMoney != (value != 0)) Log::Write(std::string("Option need_money is ") + (value ? "on: missions ask for their money" : "off: missions ask for no money"));
            g_needMoney = value != 0;
        }
        else Log::Write("Unknown option from the client: " + name);
    }

    // One "option <name> <value>" line.
    void HandleOption(const std::string& line) {
        std::istringstream words(line);
        std::string command, name;
        int value = 0;
        if (words >> command >> name >> value) Set(name, value);
        else Log::Write("Unknown message from the client: " + line);
    }
}

void Options::Install() {
    ClientLink::On("option", HandleOption);
}

bool Options::NeedMoney() { return g_needMoney; }
