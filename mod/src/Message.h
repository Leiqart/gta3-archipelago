#pragma once
#include <string>

// Shows short texts to the player at the bottom of the screen, like the game's own messages.
namespace Message {
    // Shows a text for 3 seconds, after the game's own texts already waiting; it takes the place of this mod's text still on the screen or waiting. Call it on the game thread.
    void Show(const std::string& text);
}
