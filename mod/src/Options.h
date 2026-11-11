#pragma once
#include <string>

// Our slot's options that change how the game plays, as the client sends them from the room's slot data; the game's way until it does.
namespace Options {
    // One option from an "option <name> <value>" line; call it on the game thread.
    void Set(const std::string& name, int value);
    // need_money: Bomb Da Base: Act II and The Exchange ask for their money, as in the game; true until the client says otherwise.
    bool NeedMoney();
}
