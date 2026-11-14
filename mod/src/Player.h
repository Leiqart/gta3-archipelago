#pragma once

// The player's state, which the slot's save file keeps between sessions ("player" and "progress" lines of docs/protocol.md): money, health, armour, weapons and their ammo, the weapon in hand.
// A game that starts takes the whole saved state on its first frame, before its items; then each change is sent to the GTA3 Client with the count of items applied,
// all read on the same frame, so that the save file always holds values that go together.
namespace Player {
    // Takes the client's "player" lines and hooks the game's frame; call it once, from DllMain.
    void Install();
    // Puts this weapon (eWeaponType) in the player's hands with the ammo the game's shops give with it, as soon as he is free; false for a number that is no weapon he can hold.
    // Call it on the game thread, for an item received.
    bool GiveWeapon(int weapon);
}
