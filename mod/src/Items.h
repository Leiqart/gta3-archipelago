#pragma once

// The items the room gives our slot, relayed by the GTA3 Client as "item <index> <id> <name>" (see docs/protocol.md), applied in the game.
namespace Items {
    // Takes the client's lines each frame and applies the items not applied yet; call it once, from DllMain.
    void Install();
}
