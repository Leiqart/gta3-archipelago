#pragma once

// The locations the player finishes, sent to the GTA3 Client as "check <id>" (see docs/protocol.md).
namespace Checks {
    // Listens to the missions passed and sends the checks again whenever the room connects; call it once, from DllMain, after Missions::Install.
    void Install();
}
