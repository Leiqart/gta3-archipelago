#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "ClientLink.h"
#include "Log.h"

namespace {
    // The door the GTA3 Client opens on this PC.
    constexpr const char* kHost = "127.0.0.1";
    constexpr unsigned short kPort = 2727;

    // True while the client says it is connected to the room; written by the network thread, read by the game.
    std::atomic<bool> g_connected{false};
    // The open connection to the client, so the game thread can send on it; INVALID_SOCKET while there is none.
    std::atomic<SOCKET> g_socket{INVALID_SOCKET};
    // The lines the network thread received for the game thread, which takes them each frame.
    std::mutex g_linesMutex;
    std::vector<std::string> g_lines;

    // Records the room state, and logs it only when it changes.
    void SetConnected(bool connected) {
        if (g_connected == connected) return;
        g_connected = connected;
        Log::Write(connected ? "Room connected: the button shows Run Archipelago" : "Room disconnected: the button shows Not connected");
    }

    // Applies one line received from the client: the status here, any other line is left for the game thread.
    void HandleLine(const std::string& line) {
        if (line == "status connected") SetConnected(true);
        else if (line == "status disconnected") SetConnected(false);
        else {
            const std::lock_guard<std::mutex> lock(g_linesMutex);
            g_lines.push_back(line);
        }
    }

    // Opens a TCP connection to the GTA3 Client; returns INVALID_SOCKET when the client is not running.
    SOCKET ConnectToClient() {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(kPort);
        inet_pton(AF_INET, kHost, &address.sin_addr);
        if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        return sock;
    }

    // Reads the client's lines until the connection ends; TCP is a stream, so we cut it at each '\n'.
    void ReadLines(SOCKET sock) {
        std::string buffer;
        char chunk[512];
        for (;;) {
            const int received = recv(sock, chunk, sizeof(chunk), 0);
            if (received <= 0) return;  // 0: the client closed the connection; below 0: the connection was lost.
            buffer.append(chunk, static_cast<std::size_t>(received));
            std::size_t end;
            while ((end = buffer.find('\n')) != std::string::npos) {
                HandleLine(buffer.substr(0, end));
                buffer.erase(0, end + 1);
            }
        }
    }

    // The network thread: connects to the client, reads its messages, and retries every 2 seconds while it is missing.
    DWORD WINAPI NetworkThread(void*) {
        WSADATA winsock;
        if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
            Log::Write("Winsock failed to start: no connection to the GTA3 Client");
            return 1;
        }
        bool waitingLogged = false;
        for (;;) {
            SOCKET sock = ConnectToClient();
            if (sock == INVALID_SOCKET) {
                if (!waitingLogged) Log::Write("GTA3 Client not running; retrying every 2 seconds");
                waitingLogged = true;
                Sleep(2000);
                continue;
            }
            Log::Write("Connected to the GTA3 Client");
            waitingLogged = false;
            g_socket = sock;
            ReadLines(sock);
            g_socket = INVALID_SOCKET;
            closesocket(sock);
            Log::Write("Lost the GTA3 Client");
            SetConnected(false);
        }
    }
}

void ClientLink::Start() {
    CreateThread(nullptr, 0, NetworkThread, nullptr, 0, nullptr);
}

bool ClientLink::IsConnected() {
    return g_connected;
}

bool ClientLink::Send(const std::string& message) {
    const SOCKET sock = g_socket;
    if (sock == INVALID_SOCKET) return false;
    const std::string line = message + "\n";
    return send(sock, line.data(), static_cast<int>(line.size()), 0) == static_cast<int>(line.size());
}

std::vector<std::string> ClientLink::TakeLines() {
    const std::lock_guard<std::mutex> lock(g_linesMutex);
    return std::exchange(g_lines, {});
}
