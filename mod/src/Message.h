#pragma once
#include <string>

// Shows short texts to the player at the bottom of the screen, like the game's own messages.
namespace Message {
    // Shows a text for 3 seconds, after the ones already waiting; call it on the game thread.
    void Show(const std::string& text);
}
