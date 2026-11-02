#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace Connection {
    constexpr const char* kHost = "127.0.0.1";
    constexpr unsigned short kPort = 2727;
    constexpr int kProtocolVersion = 1;

    // Creates the worker that connects once DllMain has returned.
    bool Start(HMODULE module);

    // Asks the worker to stop and closes the socket on a normal unload.
    void Stop(bool processTerminating);

    // Tells whether the local TCP socket is currently open.
    bool TcpConnected();

    // Tells whether GTA3 Client accepted the protocol version.
    bool ProtocolReady();

    // Tells whether GTA3 Client itself is authenticated on an Archipelago room.
    bool ServerConnected();

    // Called by the game thread; all actual socket I/O stays on the worker.
    bool Send(const std::string& json);
    std::vector<std::string> DrainMessages();
    void Reconnect();
}
