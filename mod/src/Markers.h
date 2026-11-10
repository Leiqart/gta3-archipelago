#pragma once

// Shows only the unlocked mission markers: a locked one is hidden, and walking onto it starts nothing.
namespace Markers {
    // Hooks the game's frame; call it once, from DllMain.
    void Install();
    // Unlocks a mission by its number in main.scm: its marker shows and starts it once its trigger runs; call it on the game thread.
    void Unlock(unsigned char number);
}
