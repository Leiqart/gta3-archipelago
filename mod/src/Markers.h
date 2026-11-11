#pragma once

// Shows only the unlocked mission markers: a locked one is hidden, and walking onto it starts nothing; an unlocked one works in any order, its trigger started and its contact marked if the story has not done it yet.
namespace Markers {
    // Hooks the game's frame; call it once, from DllMain.
    void Install();
    // Unlocks a mission by its number in main.scm: its trigger runs (started by this module if need be) and its marker shows; call it on the game thread.
    void Unlock(unsigned char number);
    // True when the mission with this number can be started now: unlocked and its trigger waiting on its marker; true too for a mission this module leaves alone.
    bool IsPlayable(unsigned char number);
}
