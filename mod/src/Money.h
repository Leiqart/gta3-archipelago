#pragma once

// The player's money, which the slot's save file keeps between sessions ("money" and "progress" lines of docs/protocol.md): a game that starts takes the saved money,
// then each change is sent to the GTA3 Client with the count of items applied, both read on the same frame, so that the save file always holds a pair that goes together.
namespace Money {
    // Takes the client's "money" lines and hooks the game's frame; call it once, from DllMain.
    void Install();
}
