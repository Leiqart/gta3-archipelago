#include <string>
#include "Log.h"
#include "Options.h"

namespace {
    bool g_needMoney = true;
}

void Options::Set(const std::string& name, int value) {
    if (name == "need_money") {
        if (g_needMoney != (value != 0)) Log::Write(std::string("Option need_money is ") + (value ? "on: missions ask for their money" : "off: missions ask for no money"));
        g_needMoney = value != 0;
    }
    else Log::Write("Unknown option from the client: " + name);
}

bool Options::NeedMoney() { return g_needMoney; }
