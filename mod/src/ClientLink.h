#pragma once
#include <string>
#include <vector>

// Our link to the GTA3 Client on 127.0.0.1:2727 (see docs/protocol.md); all of the mod's networking lives here.
namespace ClientLink {
    // Starts the network thread, which connects to the client and keeps retrying while it is missing.
    void Start();
    // True while the client says it is connected to the room.
    bool IsConnected();
    // Sends one line to the client (see docs/protocol.md); false when the client is not running.
    bool Send(const std::string& message);
    // The client's lines other than its status (the items), received since the last call; call it on the game thread, which applies them.
    std::vector<std::string> TakeLines();
}
