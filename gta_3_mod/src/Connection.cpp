#include <winsock2.h>
#include <ws2tcpip.h>

#include "Connection.h"
#include "Log.h"

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <string>
#include <deque>
#include <mutex>
#include "../third_party/nlohmann/json.hpp"

#pragma comment(lib, "Ws2_32.lib")

namespace {
    constexpr DWORD kReconnectDelayMs = 2000;
    constexpr DWORD kSocketTimeoutMs = 500;
    constexpr DWORD kHandshakeTimeoutMs = 5000;
    constexpr std::size_t kMaxMessageBytes = 16 * 1024;
    constexpr std::size_t kMaxBufferBytes = 64 * 1024;

    HANDLE g_stopEvent = nullptr;
    HANDLE g_worker = nullptr;
    std::atomic<SOCKET> g_socket{INVALID_SOCKET};
    std::atomic<bool> g_tcpConnected{false};
    std::atomic<bool> g_protocolReady{false};
    std::atomic<bool> g_serverConnected{false};
    std::mutex g_queueMutex;
    std::deque<std::string> g_incoming, g_outgoing;
    constexpr std::size_t kMaxQueuedMessages = 12000;

    bool EnqueueIncoming(const std::string& message) {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_incoming.size() >= kMaxQueuedMessages) return false;
        g_incoming.push_back(message);
        return true;
    }

    // Optionally waits, then tells whether a stop was requested.
    bool StopRequested(DWORD timeoutMs = 0) {
        return g_stopEvent &&
               WaitForSingleObject(g_stopEvent, timeoutMs) == WAIT_OBJECT_0;
    }

    // Resets every flag exposed to the future game code to false.
    void ResetStatus() {
        g_tcpConnected.store(false);
        g_protocolReady.store(false);
        g_serverConnected.store(false);
    }

    // Closes the socket owned by the worker and resets its status.
    void CloseSocket() {
        const SOCKET socket = g_socket.exchange(INVALID_SOCKET);
        if (socket != INVALID_SOCKET) {
            shutdown(socket, SD_BOTH);
            closesocket(socket);
        }
        ResetStatus();
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_incoming.clear();
        g_outgoing.clear();
        g_incoming.push_back("{\"type\":\"status\",\"server_connected\":false}");
    }

    // Finds where the value of a simple JSON key starts.
    std::size_t FindJsonValue(const std::string& json, const char* key) {
        const std::string pattern = std::string("\"") + key + "\"";
        std::size_t position = json.find(pattern);
        if (position == std::string::npos) {
            return position;
        }
        position += pattern.size();
        while (position < json.size() &&
               std::isspace(static_cast<unsigned char>(json[position]))) {
            ++position;
        }
        if (position >= json.size() || json[position] != ':') {
            return std::string::npos;
        }
        ++position;
        while (position < json.size() &&
               std::isspace(static_cast<unsigned char>(json[position]))) {
            ++position;
        }
        return position;
    }

    // Reads a JSON string; an empty string also means the value is missing.
    std::string ReadJsonString(const std::string& json, const char* key) {
        std::size_t position = FindJsonValue(json, key);
        if (position == std::string::npos || position >= json.size() ||
            json[position] != '"') {
            return {};
        }
        ++position;
        std::string value;
        bool escaped = false;
        for (; position < json.size(); ++position) {
            const char current = json[position];
            if (escaped) {
                value.push_back(current);
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                return value;
            } else {
                value.push_back(current);
            }
        }
        return {};
    }

    // Reads a JSON integer, or returns the fallback when the key is missing.
    int ReadJsonInt(const std::string& json, const char* key, int fallback) {
        const std::size_t position = FindJsonValue(json, key);
        return position == std::string::npos
            ? fallback
            : std::atoi(json.c_str() + position);
    }

    // Reads a JSON boolean, or returns the fallback when its format is invalid.
    bool ReadJsonBool(const std::string& json, const char* key, bool fallback) {
        const std::size_t position = FindJsonValue(json, key);
        if (position != std::string::npos && json.compare(position, 4, "true") == 0) {
            return true;
        }
        if (position != std::string::npos && json.compare(position, 5, "false") == 0) {
            return false;
        }
        return fallback;
    }

    // Bound the connection attempt so stopping never waits for TCP's long timeout.
    SOCKET OpenSocket() {
        SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET) {
            return INVALID_SOCKET;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(Connection::kPort);
        u_long nonBlocking = 1;
        ioctlsocket(socket, FIONBIO, &nonBlocking);
        if (inet_pton(AF_INET, Connection::kHost, &address.sin_addr) != 1) {
            closesocket(socket);
            return INVALID_SOCKET;
        }
        const int result = connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        if (result == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) { closesocket(socket); return INVALID_SOCKET; }
            fd_set writable, failed;
            FD_ZERO(&writable); FD_SET(socket, &writable);
            FD_ZERO(&failed); FD_SET(socket, &failed);
            timeval timeout{0, 100000};
            const int selected = select(0, nullptr, &writable, &failed, &timeout);
            int error = 0, size = sizeof(error);
            if (selected <= 0 || FD_ISSET(socket, &failed) || StopRequested() ||
                getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0 || error) {
                closesocket(socket); return INVALID_SOCKET;
            }
        }
        nonBlocking = 0;
        ioctlsocket(socket, FIONBIO, &nonBlocking);
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&kSocketTimeoutMs),
                   sizeof(kSocketTimeoutMs));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&kSocketTimeoutMs),
                   sizeof(kSocketTimeoutMs));
        return socket;
    }

    // Sends a whole JSON line even when Winsock splits it.
    bool SendLine(SOCKET socket, const std::string& line) {
        std::size_t offset = 0;
        while (offset < line.size() && !StopRequested()) {
            const int sent = send(socket, line.data() + offset,
                                  static_cast<int>(line.size() - offset), 0);
            if (sent <= 0) {
                return false;
            }
            offset += static_cast<std::size_t>(sent);
        }
        return offset == line.size();
    }

    // Validates the messages used by the first milestone and ignores the known future ones.
    bool HandleMessage(const std::string& json) {
        // Validate complete JSON, including escaped and Unicode room identities.
        const auto parsed = nlohmann::json::parse(json, nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object() ||
            !parsed.contains("type") || !parsed["type"].is_string()) return false;
        const std::string type = ReadJsonString(json, "type");
        if (type == "welcome") {
            if (ReadJsonString(json, "game") != "gta3" ||
                ReadJsonInt(json, "version", -1) != Connection::kProtocolVersion) {
                Log::Write("Handshake refuse: protocole incompatible");
                return false;
            }
            g_protocolReady.store(true);
            Log::Write("Handshake GTA3 accepte (protocole v%d)",
                       Connection::kProtocolVersion);
            return true;
        }
        if (!g_protocolReady.load()) {
            Log::Write("Message '%s' recu avant welcome", type.c_str());
            return false;
        }
        if (type == "status") {
            const bool connected = ReadJsonBool(json, "server_connected", false);
            const bool previous = g_serverConnected.exchange(connected);
            if (connected != previous) {
                Log::Write("Serveur Archipelago connecte=%d", connected ? 1 : 0);
            }
            return EnqueueIncoming(json);
        }
        if (type == "error") {
            Log::Write("Erreur du GTA3 Client: %s", ReadJsonString(json, "code").c_str());
            return EnqueueIncoming(json);
        }
        if (type == "pong" || type == "room" || type == "sync_start" ||
            type == "sync_end" || type == "checked" || type == "item" ||
            type == "check_ack" || type == "goal_ack") {
            return EnqueueIncoming(json);
        }
        Log::Write("Type de message inconnu: '%s'", type.c_str());
        return false;
    }

    // Performs the handshake, then reads JSON Lines messages until disconnection.
    bool RunSession(SOCKET socket) {
        const std::string hello =
            "{\"type\":\"hello\",\"game\":\"gta3\",\"version\":" +
            std::to_string(Connection::kProtocolVersion) + "}\n";
        if (!SendLine(socket, hello)) {
            return false;
        }

        const DWORD handshakeStarted = GetTickCount();
        std::string buffer;
        while (!StopRequested()) {
            std::deque<std::string> outgoing;
            if (g_protocolReady.load()) {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                outgoing.swap(g_outgoing);
            }
            for (const auto& message : outgoing)
                if (!SendLine(socket, message + "\n")) return false;
            fd_set readable;
            FD_ZERO(&readable); FD_SET(socket, &readable);
            timeval poll{0, 20000};
            const int selected = select(0, &readable, nullptr, nullptr, &poll);
            if (selected == SOCKET_ERROR) return false;
            if (!g_protocolReady.load() && GetTickCount() - handshakeStarted >= kHandshakeTimeoutMs)
                return false;
            if (selected == 0) continue;
            char chunk[4096];
            const int received = recv(socket, chunk, sizeof(chunk), 0);
            if (received == SOCKET_ERROR &&
                (WSAGetLastError() == WSAETIMEDOUT ||
                 WSAGetLastError() == WSAEWOULDBLOCK)) {
                if (!g_protocolReady.load() &&
                    GetTickCount() - handshakeStarted >= kHandshakeTimeoutMs) {
                    Log::Write("Delai du handshake depasse");
                    return false;
                }
                continue;
            }
            if (received <= 0) {
                return false;
            }
            buffer.append(chunk, static_cast<std::size_t>(received));
            if (buffer.size() > kMaxBufferBytes) {
                Log::Write("Tampon entrant trop grand");
                return false;
            }

            std::size_t newline = std::string::npos;
            while ((newline = buffer.find('\n')) != std::string::npos) {
                if (newline > kMaxMessageBytes) {
                    Log::Write("Message entrant trop grand");
                    return false;
                }
                std::string message = buffer.substr(0, newline);
                buffer.erase(0, newline + 1);
                if (!message.empty() && message.back() == '\r') {
                    message.pop_back();
                }
                if (message.empty() || !HandleMessage(message)) {
                    return false;
                }
            }
            if (buffer.size() > kMaxMessageBytes) {
                Log::Write("Message entrant non termine trop grand");
                return false;
            }
        }
        return true;
    }

    // Owns Winsock and the socket for the whole plugin lifetime.
    DWORD WINAPI WorkerMain(void* parameter) {
        const HMODULE module = static_cast<HMODULE>(parameter);
        (void)module; // The ASI bootstrap opens the log before starting the worker.
        Log::Write("GTA3Archipelago charge; connexion locale %s:%u",
                   Connection::kHost, static_cast<unsigned int>(Connection::kPort));

        WSADATA data{};
        const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
        if (startupResult != 0) {
            Log::Write("WSAStartup a echoue: %d", startupResult);
            return 1;
        }

        bool waitingAnnounced = false;
        while (!StopRequested()) {
            const SOCKET socket = OpenSocket();
            if (socket == INVALID_SOCKET) {
                if (!waitingAnnounced) {
                    Log::Write("En attente de GTA3 Client sur %s:%u",
                               Connection::kHost,
                               static_cast<unsigned int>(Connection::kPort));
                    waitingAnnounced = true;
                }
                StopRequested(kReconnectDelayMs);
                continue;
            }

            waitingAnnounced = false;
            g_socket.store(socket);
            g_tcpConnected.store(true);
            Log::Write("Connexion TCP locale etablie");
            RunSession(socket);
            CloseSocket();
            if (!StopRequested()) {
                Log::Write("Connexion locale perdue; nouvelle tentative dans %u ms",
                           static_cast<unsigned int>(kReconnectDelayMs));
                StopRequested(kReconnectDelayMs);
            }
        }

        CloseSocket();
        WSACleanup();
        Log::Write("Service de connexion arrete");
        return 0;
    }
}

// Creates only the Windows objects needed for the deferred worker start.
bool Connection::Start(HMODULE module) {
    if (g_worker) {
        return true;
    }
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_incoming.clear(); g_outgoing.clear();
    }
    g_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        return false;
    }
    g_worker = CreateThread(nullptr, 0, WorkerMain, module, 0, nullptr);
    if (!g_worker) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return false;
    }
    return true;
}

// Signals the stop, unblocks recv and waits for the worker unless the process is exiting.
void Connection::Stop(bool processTerminating) {
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }
    const SOCKET socket = g_socket.load();
    if (socket != INVALID_SOCKET) {
        shutdown(socket, SD_BOTH);
    }
    if (processTerminating) {
        return;
    }
    if (g_worker) {
        WaitForSingleObject(g_worker, 2000);
        CloseHandle(g_worker);
        g_worker = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
    ResetStatus();
}

// Returns the atomic state of the local socket.
bool Connection::TcpConnected() {
    return g_tcpConnected.load();
}

// Returns the atomic state of the local handshake.
bool Connection::ProtocolReady() {
    return g_protocolReady.load();
}

// Returns the atomic state of the link between GTA3 Client and the room.
bool Connection::ServerConnected() {
    return g_serverConnected.load();
}

bool Connection::Send(const std::string& json) {
    if (json.empty() || json.size() > kMaxMessageBytes || !ProtocolReady()) return false;
    std::lock_guard<std::mutex> lock(g_queueMutex);
    if (g_outgoing.size() >= kMaxQueuedMessages) return false;
    g_outgoing.push_back(json);
    return true;
}

std::vector<std::string> Connection::DrainMessages() {
    std::lock_guard<std::mutex> lock(g_queueMutex);
    std::vector<std::string> messages(g_incoming.begin(), g_incoming.end());
    g_incoming.clear();
    return messages;
}

void Connection::Reconnect() {
    const auto socket = g_socket.load();
    if (socket != INVALID_SOCKET) shutdown(socket, SD_BOTH);
}
