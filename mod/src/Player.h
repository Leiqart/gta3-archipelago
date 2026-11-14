#pragma once

// The player's state, which the slot's save file keeps between sessions ("player" and "progress" lines of docs/protocol.md): money, health, armour, weapons and their ammo, the weapon in hand.
// A game that starts takes the saved state; then each change is sent to the GTA3 Client with the count of items applied, all read on the same frame, so that the save file always holds values that go together.
namespace Player {
    // Takes the client's "player" lines and hooks the game's frame; call it once, from DllMain.
    void Install();
}
