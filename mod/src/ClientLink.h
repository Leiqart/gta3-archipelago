#pragma once
#include <functional>
#include <string>

// Our link to the GTA3 Client on 127.0.0.1:2727 (see docs/protocol.md); all of the mod's networking lives here.
namespace ClientLink {
    // Starts the network thread, which connects to the client and keeps retrying while it is missing.
    void Start();
    // True while the client says it is connected to the room.
    bool IsConnected();
    // Sends one line to the client (see docs/protocol.md); false when the client is not running.
    bool Send(const std::string& message);
    // Hands every line from the client starting with this command ("item", "option", "kill") to this handler, on the game thread; call it from a module's Install.
    void On(const std::string& command, std::function<void(const std::string& line)> handler);
    // Hooks the game's frame, where the handlers run; call it once, from DllMain.
    void Install();
}
