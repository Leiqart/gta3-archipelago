#pragma once

// Our slot's options that change how the game plays, as the client sends them from the room's slot data; the game's way until it does.
namespace Options {
    // Takes the client's "option <name> <value>" lines; call it once, from DllMain.
    void Install();
    // need_money: Bomb Da Base: Act II and The Exchange ask for their money, as in the game; true until the client says otherwise.
    bool NeedMoney();
}
