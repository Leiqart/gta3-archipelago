#pragma once

// The main menu's Start Game button shows the connection state and only starts a new game when connected; the pause menu has none.
namespace Menu {
    // Checks the menu table, hooks the game's text function and removes the pause menu's Start Game row; MinHook must be started first.
    bool Install();
    // Sets what the button does for the current connection state; called every 50 ms.
    void Update();
}
