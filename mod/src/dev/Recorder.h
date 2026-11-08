#pragma once
#include <string>

// Dev builds only: while the player corrects the autopilot by hand, writes their moves and game actions to the log, to turn them into scenario steps.
namespace Recorder {
    // Starts recording, with the reason written in the log; does nothing if it already records.
    void Start(const std::string& why);
    // Stops recording, with a last position line.
    void Stop();
    // Writes what changed since the previous frame; call it every frame on the game thread.
    void Update();
}
