#pragma once

// Shows only the unlocked mission markers: a locked one is hidden, and walking onto it starts nothing.
namespace Markers {
    // Hooks the game's frame; call it once, from DllMain.
    void Install();
}
