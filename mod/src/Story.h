#pragma once

// Takes off the story's locks that only hold for some order of the missions, so any unlocked mission can be played whatever was passed before.
namespace Story {
    // Hooks the game's script start and frame; call it once, from DllMain.
    void Install();
}
