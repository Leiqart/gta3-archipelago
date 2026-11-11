#pragma once

// Takes off the story's locks that only hold for some order of the missions, so any unlocked mission can be played whatever was passed before, and the money fees when need_money is off.
namespace Story {
    // Hooks the game's script start and frame; call it once, from DllMain.
    void Install();
}
