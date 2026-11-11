#pragma once

// DeathLink in the game: tells the client each time the player is wasted or busted, and kills the player when the client says another player died.
namespace Deaths {
    // Hooks the game's frame and the client's "kill <name>" lines; call it once, from DllMain.
    void Install();
}
