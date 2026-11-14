#pragma once

// The items the room gives our slot, relayed by the GTA3 Client as "item <index> <effect> <value> <name>" (see docs/protocol.md), applied in the game.
// The slot's save file keeps how many items its games applied ("given_index" lines): a game that starts applies the lasting ones again and skips the consumable ones ("consumables" lines).
namespace Items {
    // Takes the client's "consumables", "given_index" and "item" lines and applies the items not applied yet; call it once, from DllMain.
    void Install();
    // How many items of the slot's list its games have applied so far, this one and the earlier ones: what the save file must keep, with the player's state (Player sends both).
    int Applied();
}
