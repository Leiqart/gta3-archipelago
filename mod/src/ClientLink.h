#pragma once

// Our link to the GTA3 Client on 127.0.0.1:2727 (see docs/protocol.md); all of the mod's networking lives here.
namespace ClientLink {
    // Starts the network thread, which connects to the client and keeps retrying while it is missing.
    void Start();
    // True while the client says it is connected to the room.
    bool IsConnected();
}
