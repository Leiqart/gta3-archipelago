#pragma once

// The missions the player passes, sent to the GTA3 Client as "check <key>" (see docs/protocol.md), which knows which ones are locations.
namespace Checks {
    // Listens to the missions passed and sends the checks again whenever the room connects; call it once, from DllMain, after Missions::Install.
    void Install();
}
