#pragma once

// The weapons the shops sell. Those that are items in our slot ("weapons" lines of docs/protocol.md) are out of stock until the slot has received them, then on sale, whatever the story says; the others are sold as in the game.
namespace Shops {
    // Takes the client's "weapons" lines and hooks the game's frame; call it once, from DllMain.
    void Install();
    // Lets the shops sell the weapon with this number in the game (eWeaponType): our slot has received its item; call it on the game thread.
    void Unlock(int weapon);
}
