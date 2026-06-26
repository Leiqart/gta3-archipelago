#include <windows.h>
#include <DbgHelp.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <eh.h>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <apclient.hpp>

#include "ApBridgeProcess.h"
#include "ApConfig.h"
#include "ApData.h"
#include "JsonLite.h"
#include "Logger.h"
#include "PluginPaths.h"

namespace {
    constexpr char kBridgeExeName[] = "GTA3_AP";
    constexpr char kBridgeLogFilename[] = "GTA3_AP.log";
    constexpr char kBridgeStdoutFilename[] = "GTA3_AP.stdout.log";
    constexpr char kBridgeCaBundleFilename[] = "GTA3_AP.cacert.pem";
    constexpr char kGameName[] = "Grand Theft Auto III";
    constexpr char kStateFilename[] = "III.Archipelago.state.json";
    constexpr char kItemsFilename[] = "III.Archipelago.items.json";
    constexpr char kBridgeFilename[] = "III.Archipelago.bridge.json";
    constexpr char kStatusFilename[] = "III.Archipelago.status.json";
    // Run-state persistence channel (Archipelago data storage). The plugin's
    // RunState file is mirrored into the slot's data storage so the run roams
    // with the multiworld room: download wins at connect, then local changes
    // are pushed up. III.Archipelago.sync.json tells the plugin to reload.
    constexpr char kRunStateIniFilename[] = "III.MissionSelector.state.ini";
    constexpr char kSyncFilename[] = "III.Archipelago.sync.json";
    // Bridge memory of the last room seed; a mismatch at RoomInfo means the
    // local files belong to ANOTHER room and must be reset before any push.
    constexpr char kRoomFilename[] = "III.Archipelago.room.json";
    constexpr char kRunStateStorageKeyPrefix[] = "gta3_runstate_";

    struct Endpoint {
        bool        secure = false;
        std::string host;
        unsigned short port = 0;
        std::string path = "/";
    };

    std::wstring ToWide(const std::string& value) {
        if (value.empty()) {
            return std::wstring();
        }
        const int count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (count <= 0) {
            return std::wstring(value.begin(), value.end());
        }
        std::wstring out(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), count);
        if (!out.empty() && out.back() == L'\0') {
            out.pop_back();
        }
        return out;
    }

    std::string ReadTextFile(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            return std::string();
        }
        std::ostringstream ss;
        ss << input.rdbuf();
        return ss.str();
    }

    bool WriteTextFile(const std::string& path, const std::string& text) {
        FILE* file = nullptr;
        fopen_s(&file, path.c_str(), "wb");
        if (!file) {
            return false;
        }
        const bool ok = std::fwrite(text.data(), 1, text.size(), file) == text.size();
        std::fclose(file);
        return ok;
    }

    std::string Trim(const std::string& value) {
        std::size_t start = 0;
        while (start < value.size() &&
               std::isspace(static_cast<unsigned char>(value[start])) != 0) {
            ++start;
        }
        std::size_t end = value.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
            --end;
        }
        return value.substr(start, end - start);
    }

    std::string BuildUuid() {
        char computerName[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
        if (!GetComputerNameA(computerName, &len)) {
            std::snprintf(computerName, sizeof(computerName), "pc");
        }
        const std::string base = PluginPaths::InGameDir("") + "|" + computerName;
        char buf[64] = {};
        std::snprintf(buf, sizeof(buf), "gta3-%08X", ApBridgeProcess::HashLower(base));
        return buf;
    }

    bool IsUsableFile(const std::string& path) {
        const DWORD attrs = GetFileAttributesA(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES &&
               (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool ParseServerSpec(const std::string& raw, bool forceSecure, Endpoint& out) {
        std::string value = Trim(raw);
        if (value.empty()) {
            return false;
        }

        bool secure = forceSecure;
        std::string rest = value;
        const std::size_t scheme = value.find("://");
        if (scheme != std::string::npos) {
            const std::string prefix = value.substr(0, scheme);
            secure = _stricmp(prefix.c_str(), "wss") == 0;
            rest = value.substr(scheme + 3);
        }

        std::string hostPort = rest;
        std::string path = "/";
        const std::size_t slash = rest.find('/');
        if (slash != std::string::npos) {
            hostPort = rest.substr(0, slash);
            path = rest.substr(slash);
            if (path.empty()) {
                path = "/";
            }
        }

        std::string host = hostPort;
        unsigned short port = static_cast<unsigned short>(secure ? 443 : 80);
        const std::size_t colon = hostPort.rfind(':');
        if (colon != std::string::npos) {
            host = hostPort.substr(0, colon);
            const int parsedPort = std::atoi(hostPort.c_str() + colon + 1);
            if (parsedPort > 0 && parsedPort <= 65535) {
                port = static_cast<unsigned short>(parsedPort);
            }
        }

        host = Trim(host);
        if (host.empty()) {
            return false;
        }

        out.secure = secure;
        out.host = host;
        out.port = port;
        out.path = path;
        return true;
    }

    bool EqualsIgnoreCase(const std::string& lhs, const char* rhs) {
        return _stricmp(lhs.c_str(), rhs) == 0;
    }

    bool IsLocalHost(const std::string& host) {
        return EqualsIgnoreCase(host, "localhost") ||
               EqualsIgnoreCase(host, "127.0.0.1") ||
               EqualsIgnoreCase(host, "::1");
    }

    std::string BuildUriFromEndpoint(const Endpoint& endpoint) {
        std::ostringstream ss;
        ss << (endpoint.secure ? "wss://" : "ws://")
           << endpoint.host << ":" << endpoint.port;
        if (endpoint.path.empty()) {
            ss << "/";
        } else {
            if (endpoint.path.front() != '/') {
                ss << "/";
            }
            ss << endpoint.path;
        }
        return ss.str();
    }

    std::string NormalizeServerUri(const std::string& rawServer) {
        const std::string server = Trim(rawServer);
        if (server.empty()) {
            return std::string();
        }

        if (server.find("://") != std::string::npos) {
            Endpoint endpoint;
            return ParseServerSpec(server, false, endpoint) ? BuildUriFromEndpoint(endpoint) : server;
        }

        Endpoint probe;
        const bool haveProbe = ParseServerSpec(server, false, probe);
        const bool preferSecure = !(haveProbe && IsLocalHost(probe.host));

        Endpoint endpoint;
        return ParseServerSpec(server, preferSecure, endpoint)
            ? BuildUriFromEndpoint(endpoint)
            : server;
    }

    std::string ResolveCaBundlePath() {
        const std::string inGame = PluginPaths::InGameDir(kBridgeCaBundleFilename);
        if (IsUsableFile(inGame)) {
            return inGame;
        }

        const std::string inModule = PluginPaths::InModuleDir(kBridgeCaBundleFilename);
        if (IsUsableFile(inModule)) {
            return inModule;
        }

        const char* fallbacks[] = {
            "C:\\Program Files\\Git\\usr\\ssl\\cert.pem",
            "C:\\Program Files\\Git\\mingw64\\ssl\\certs\\ca-bundle.crt",
            "C:\\Program Files\\Git\\mingw64\\bin\\curl-ca-bundle.crt",
        };
        for (const char* fallback : fallbacks) {
            if (IsUsableFile(fallback)) {
                return fallback;
            }
        }

        return std::string();
    }

    bool SearchExecutablePath(const char* name, std::string& out) {
        if (!name || name[0] == '\0') {
            return false;
        }
        char buf[MAX_PATH] = {};
        const DWORD length = SearchPathA(nullptr, name, nullptr, MAX_PATH, buf, nullptr);
        if (length == 0 || length >= MAX_PATH) {
            return false;
        }
        out.assign(buf, static_cast<std::size_t>(length));
        return true;
    }

    std::string QuoteCommandArg(const std::string& value) {
        std::string out;
        out.reserve(value.size() + 8);
        out.push_back('"');
        std::size_t slashCount = 0;
        for (char c : value) {
            if (c == '\\') {
                ++slashCount;
                continue;
            }
            if (c == '"') {
                out.append(slashCount * 2 + 1, '\\');
                out.push_back('"');
                slashCount = 0;
                continue;
            }
            if (slashCount != 0) {
                out.append(slashCount, '\\');
                slashCount = 0;
            }
            out.push_back(c);
        }
        if (slashCount != 0) {
            out.append(slashCount * 2, '\\');
        }
        out.push_back('"');
        return out;
    }

    bool ShouldUsePythonBridge(const std::string& rawServer) {
        Endpoint endpoint;
        return ParseServerSpec(rawServer, false, endpoint) && !IsLocalHost(endpoint.host);
    }

    struct PythonLauncher {
        std::string executablePath;
        std::string preArgs;
    };

    bool ResolvePythonLauncher(PythonLauncher& out) {
        if (SearchExecutablePath("python.exe", out.executablePath)) {
            out.preArgs.clear();
            return true;
        }
        if (SearchExecutablePath("py.exe", out.executablePath)) {
            out.preArgs = "-3";
            return true;
        }
        return false;
    }

    bool BackendSupportsServer(const std::string& rawServer) {
        const std::string server = Trim(rawServer);
        if (server.empty()) {
            return false;
        }
#ifdef WSWRAP_NO_SSL
        if (_strnicmp(server.c_str(), "ws://", 5) == 0) {
            return true;
        }
        if (_strnicmp(server.c_str(), "wss://", 6) == 0) {
            return false;
        }
        Endpoint endpoint;
        return ParseServerSpec(server, false, endpoint) && IsLocalHost(endpoint.host);
#else
        return true;
#endif
    }

    void WriteStatus(bool connected, const std::string& slot, const std::string& server) {
        std::ostringstream ss;
        ss << "{\"connected\": " << (connected ? "true" : "false")
           << ", \"slot\": \"" << JsonLite::Escape(slot)
           << "\", \"server\": \"" << JsonLite::Escape(server) << "\"}";
        WriteTextFile(PluginPaths::InGameDir(kStatusFilename), ss.str());
    }

    void WriteItems(const std::vector<std::string>& names) {
        std::ostringstream ss;
        ss << "{\n  \"received_items\": [";
        for (std::size_t i = 0; i < names.size(); ++i) {
            ss << (i == 0 ? "" : ",") << "\n    \"" << JsonLite::Escape(names[i]) << "\"";
        }
        if (!names.empty()) {
            ss << "\n  ";
        }
        ss << "]\n}\n";
        WriteTextFile(PluginPaths::InGameDir(kItemsFilename), ss.str());
    }

    // The server hosts a different room than the local files belong to: every
    // locally-stored check, item and run value is stale and would contaminate
    // the fresh room (instant bogus checks, polluted data storage). Wipe them
    // and remember the new room's seed.
    void ResetLocalStateForNewRoom(const std::string& seed, const std::string& slot);

    void WriteSyncFile(int seq, int settled) {
        std::ostringstream ss;
        ss << "{\"seq\": " << seq << ", \"settled\": " << settled << "}";
        WriteTextFile(PluginPaths::InGameDir(kSyncFilename), ss.str());
    }

    // Signal the plugin that III.MissionSelector.state.ini was replaced from
    // the server; it reloads RunState and re-applies it to the live player.
    // A download also counts as the storage decision being settled.
    void BumpStateSyncSeq() {
        const std::string text = ReadTextFile(PluginPaths::InGameDir(kSyncFilename));
        WriteSyncFile(JsonLite::ReadInt(text, "seq", 0) + 1,
                      JsonLite::ReadInt(text, "settled", 0) + 1);
    }

    // The data-storage GET decision is final for this connect (download
    // applied, local kept, or nothing stored): the plugin may lift its
    // resume loading screen.
    void MarkStorageSettled() {
        const std::string text = ReadTextFile(PluginPaths::InGameDir(kSyncFilename));
        WriteSyncFile(JsonLite::ReadInt(text, "seq", 0),
                      JsonLite::ReadInt(text, "settled", 0) + 1);
    }

    std::map<std::string, std::string> ParseRunStateIni(const std::string& text) {
        std::map<std::string, std::string> out;
        std::istringstream input(text);
        std::string line;
        while (std::getline(input, line)) {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            out[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
        }
        return out;
    }

    std::string SanitizeRunStateIni(const std::string& text) {
        std::istringstream input(text);
        std::ostringstream out;
        std::string line;
        bool sawValidated = false;
        bool sawValidatedExtra = false;
        while (std::getline(input, line)) {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) {
                out << line << "\n";
                continue;
            }
            const std::string key = Trim(line.substr(0, eq));
            if (key == "validated") {
                out << "validated=\n";
                sawValidated = true;
            } else if (key == "validated_extra") {
                out << "validated_extra=\n";
                sawValidatedExtra = true;
            } else {
                out << line << "\n";
            }
        }
        if (!sawValidated) {
            out << "validated=\n";
        }
        if (!sawValidatedExtra) {
            out << "validated_extra=\n";
        }
        return out.str();
    }

    std::string ReadSanitizedRunStateIni(const std::string& path) {
        const std::string text = ReadTextFile(path);
        if (text.empty()) {
            return text;
        }
        const std::string sanitized = SanitizeRunStateIni(text);
        if (sanitized != text) {
            WriteTextFile(path, sanitized);
        }
        return sanitized;
    }

    int ParseIntOrDefault(const std::map<std::string, std::string>& values,
                          const char* key,
                          int defaultValue) {
        const auto it = values.find(key);
        return it == values.end() ? defaultValue : std::atoi(it->second.c_str());
    }

    nlohmann::json ParseRunStateWeapons(const std::string& value) {
        nlohmann::json weapons = nlohmann::json::array();
        std::size_t pos = 0;
        int slot = 0;
        while (pos <= value.size()) {
            std::size_t comma = value.find(',', pos);
            if (comma == std::string::npos) {
                comma = value.size();
            }
            const std::string token = Trim(value.substr(pos, comma - pos));
            const std::size_t colon = token.find(':');
            if (colon != std::string::npos) {
                weapons.push_back({
                    {"slot", slot},
                    {"type", std::atoi(token.substr(0, colon).c_str())},
                    {"ammo", std::atoi(token.c_str() + colon + 1)},
                });
            }
            ++slot;
            if (comma == value.size()) {
                break;
            }
            pos = comma + 1;
        }
        return weapons;
    }

    nlohmann::json ParseRunStatePosition(const std::string& value) {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (std::sscanf(value.c_str(), "%f,%f,%f", &x, &y, &z) != 3) {
            return nullptr;
        }
        return {{"x", x}, {"y", y}, {"z", z}};
    }

    nlohmann::json BuildRunStatePayload(const std::string& ini, double mtime) {
        const std::string sanitizedIni = SanitizeRunStateIni(ini);
        const auto values = ParseRunStateIni(sanitizedIni);
        const auto stringValue = [&values](const char* key) -> std::string {
            const auto it = values.find(key);
            return it == values.end() ? std::string() : it->second;
        };
        return {
            {"kind", "gta3_runstate"},
            {"schema_version", 2},
            {"mtime", mtime},
            {"ini", sanitizedIni},
            {"player_state", {
                {"money", ParseIntOrDefault(values, "money", 0)},
                {"weapons", ParseRunStateWeapons(stringValue("weapons"))},
                {"health", ParseIntOrDefault(values, "health", 0)},
                {"armor", ParseIntOrDefault(values, "armor", 0)},
                {"position", ParseRunStatePosition(stringValue("pos"))},
                {"packages", ParseIntOrDefault(values, "packages", 0)},
            }},
            {"progress", {
                {"run_active", ParseIntOrDefault(values, "run_active", 0) != 0},
                {"checks", ParseIntOrDefault(values, "checks", 0)},
                {"map_state", ParseIntOrDefault(values, "map_state", 0)},
                {"unlocked_chars", ParseIntOrDefault(values, "unlocked_chars", 1)},
                {"unlocked_buckets", stringValue("unlocked_buckets")},
                {"validated", stringValue("validated")},
                {"validated_extra", stringValue("validated_extra")},
            }},
        };
    }

    void ResetLocalStateForNewRoom(const std::string& seed, const std::string& slot) {
        std::ostringstream st;
        st << "{\n  \"schema_version\": 1,\n  \"slot_name\": \"" << JsonLite::Escape(slot)
           << "\",\n  \"seed_name\": \"" << JsonLite::Escape(seed)
           << "\",\n  \"run_active\": true,\n  \"start_mode\": \"archipelago\",\n"
           << "  \"received_items_applied\": 0,\n  \"locations_checked\": []\n}\n";
        WriteTextFile(PluginPaths::InGameDir(kStateFilename), st.str());
        WriteItems({});
        DeleteFileA(PluginPaths::InGameDir(kRunStateIniFilename).c_str());
        BumpStateSyncSeq();
        WriteTextFile(PluginPaths::InGameDir(kRoomFilename),
                      std::string("{\"seed_name\": \"") + JsonLite::Escape(seed) + "\"}");
        Logger::Log("New room detected (seed='%s'): local run state reset", seed.c_str());
    }

    std::set<std::string> WriteServerCheckedLocations(
        const std::set<int64_t>& checkedLocationIds,
        const std::string& slot,
        const std::string& seed) {
        std::set<std::string> checkedNames;
        for (int64_t id : checkedLocationIds) {
            if (const char* name = ApData::LookupLocationName(static_cast<int>(id))) {
                checkedNames.insert(name);
            }
        }

        const std::string stateText = ReadTextFile(PluginPaths::InGameDir(kStateFilename));
        const int schemaVersion = JsonLite::ReadInt(stateText, "schema_version", 1);
        const int applied = JsonLite::ReadInt(stateText, "received_items_applied", 0);
        const std::string existingSeed =
            JsonLite::ReadScalarString(stateText, "seed_name", "online");
        const std::string startMode =
            JsonLite::ReadScalarString(stateText, "start_mode", "archipelago");

        std::ostringstream out;
        out << "{\n"
            << "  \"schema_version\": " << schemaVersion << ",\n"
            << "  \"slot_name\": \"" << JsonLite::Escape(slot) << "\",\n"
            << "  \"seed_name\": \""
            << JsonLite::Escape(seed.empty() ? existingSeed : seed) << "\",\n"
            << "  \"run_active\": true,\n"
            << "  \"start_mode\": \"" << JsonLite::Escape(startMode) << "\",\n"
            << "  \"received_items_applied\": " << applied << ",\n"
            << "  \"locations_checked\": [";
        std::size_t index = 0;
        for (const std::string& name : checkedNames) {
            out << (index++ == 0 ? "" : ",") << "\n    \""
                << JsonLite::Escape(name) << "\"";
        }
        if (!checkedNames.empty()) {
            out << "\n  ";
        }
        out << "]\n}\n";
        WriteTextFile(PluginPaths::InGameDir(kStateFilename), out.str());
        return checkedNames;
    }

    void Toast(const std::string& message) {
        const std::string path = PluginPaths::InGameDir(kBridgeFilename);
        const std::string current = ReadTextFile(path);
        const int seq = JsonLite::ReadInt(current, "seq", 0) + 1;
        std::ostringstream ss;
        ss << "{\"seq\": " << seq << ", \"toast\": \"" << JsonLite::Escape(message) << "\"}";
        WriteTextFile(path, ss.str());
    }

    class PythonBridgeWorker {
    public:
        ~PythonBridgeWorker() {
            Stop();
        }

        bool EnsureRunning(const ApConfig::Settings& cfg) {
            const std::string desiredKey = BuildSessionKey(cfg);
            const bool configChanged = desiredKey != sessionKey_;
            if (configChanged) {
                Stop();
            } else {
                ReapIfExited();
            }

            if (process_ != nullptr) {
                return true;
            }

            const DWORD now = GetTickCount();
            if (!configChanged && lastLaunchTick_ != 0 && now - lastLaunchTick_ < 5000) {
                return false;
            }

            const std::string scriptPath = PluginPaths::InGameDir("..\\ap_bridge\\bridge.py");
            if (!IsUsableFile(scriptPath)) {
                Logger::Log("Python bridge unavailable: missing '%s'", scriptPath.c_str());
                return false;
            }

            PythonLauncher launcher;
            if (!ResolvePythonLauncher(launcher)) {
                Logger::Log("Python bridge unavailable: neither python.exe nor py.exe found in PATH");
                return false;
            }

            std::string commandLine = QuoteCommandArg(launcher.executablePath);
            if (!launcher.preArgs.empty()) {
                commandLine += " ";
                commandLine += launcher.preArgs;
            }
            commandLine += " -u ";
            commandLine += QuoteCommandArg(scriptPath);
            const std::string gameDir = PluginPaths::InGameDir("");
            commandLine += " --game-dir ";
            commandLine += QuoteCommandArg(gameDir);

            std::vector<char> mutableCommand(commandLine.begin(), commandLine.end());
            mutableCommand.push_back('\0');

            STARTUPINFOA si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            const std::string workingDir = PluginPaths::InGameDir("");
            const BOOL ok = CreateProcessA(
                launcher.executablePath.c_str(),
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                workingDir.c_str(),
                &si,
                &pi);
            lastLaunchTick_ = now;
            if (!ok) {
                Logger::Log("Python bridge launch failed for '%s' (err=%lu)",
                            launcher.executablePath.c_str(), GetLastError());
                return false;
            }

            process_ = pi.hProcess;
            CloseHandle(pi.hThread);
            sessionKey_ = desiredKey;
            slot_ = cfg.slot;
            server_ = cfg.server;
            Logger::Log("Started python bridge pid=%lu for server='%s' slot='%s'",
                        pi.dwProcessId, cfg.server.c_str(), cfg.slot.c_str());
            return true;
        }

        void Stop() {
            if (!process_) {
                sessionKey_.clear();
                return;
            }
            if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) {
                TerminateProcess(process_, 0);
                WaitForSingleObject(process_, 2000);
            }
            CloseHandle(process_);
            process_ = nullptr;
            sessionKey_.clear();
            lastLaunchTick_ = 0;
            WriteStatus(false, slot_, server_);
        }

    private:
        static std::string BuildSessionKey(const ApConfig::Settings& cfg) {
            return cfg.server + "\n" + cfg.slot + "\n" + cfg.password;
        }

        void ReapIfExited() {
            if (!process_) {
                return;
            }
            if (WaitForSingleObject(process_, 0) != WAIT_OBJECT_0) {
                return;
            }
            DWORD exitCode = 0;
            GetExitCodeProcess(process_, &exitCode);
            Logger::Log("Python bridge exited with code=%lu", exitCode);
            CloseHandle(process_);
            process_ = nullptr;
            WriteStatus(false, slot_, server_);
        }

        HANDLE      process_ = nullptr;
        DWORD       lastLaunchTick_ = 0;
        std::string sessionKey_;
        std::string slot_;
        std::string server_;
    };

    void LogActiveException(const char* prefix) {
        try {
            const std::exception_ptr ex = std::current_exception();
            if (!ex) {
                Logger::Log("%s: no active exception", prefix);
                return;
            }
            std::rethrow_exception(ex);
        } catch (const std::exception& ex) {
            Logger::Log("%s: %s", prefix, ex.what());
        } catch (...) {
            Logger::Log("%s: unknown", prefix);
        }
    }

    void LogBacktrace(const char* prefix) {
        HANDLE process = GetCurrentProcess();
        void* frames[16] = {};
        const USHORT count = CaptureStackBackTrace(0, 16, frames, nullptr);

        unsigned char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        for (USHORT i = 0; i < count; ++i) {
            DWORD64 displacement = 0;
            DWORD lineDisplacement = 0;
            const DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
            const BOOL haveSymbol = SymFromAddr(process, address, &displacement, symbol);
            const BOOL haveLine = SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo);
            if (haveSymbol && haveLine) {
                Logger::Log("%s[%u]=%s+0x%llX (%s:%lu)",
                            prefix,
                            static_cast<unsigned>(i),
                            symbol->Name,
                            static_cast<unsigned long long>(displacement),
                            lineInfo.FileName,
                            lineInfo.LineNumber);
            } else if (haveSymbol) {
                Logger::Log("%s[%u]=%s+0x%llX",
                            prefix,
                            static_cast<unsigned>(i),
                            symbol->Name,
                            static_cast<unsigned long long>(displacement));
            } else {
                Logger::Log("%s[%u]=%p", prefix, static_cast<unsigned>(i), frames[i]);
            }
        }
    }

    LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* info) {
        const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
        const void* address = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
        Logger::Log("Unhandled SEH exception code=0x%08lX address=%p", code, address);
        LogBacktrace("  seh_bt");
        Logger::Shutdown();
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void TerminateHandler() {
        LogActiveException("std::terminate");
        Logger::Shutdown();
        ExitProcess(250);
    }

    void __cdecl PurecallHandler() {
        Logger::Log("Pure virtual call detected");
        Logger::Shutdown();
        ExitProcess(251);
    }

    void __cdecl InvalidParameterHandler(const wchar_t* expression,
                                         const wchar_t* function,
                                         const wchar_t* file,
                                         unsigned int line,
                                         uintptr_t) {
        const std::wstring expr = expression ? expression : L"";
        const std::wstring func = function ? function : L"";
        const std::wstring path = file ? file : L"";
        Logger::Log("Invalid CRT parameter detected: func='%S' file='%S' line=%u expr='%S'",
                    func.c_str(), path.c_str(), line, expr.c_str());
        LogBacktrace("  crt_bt");
        Logger::Shutdown();
        ExitProcess(252);
    }

    void __cdecl AbortSignalHandler(int signal) {
        Logger::Log("Abort signal received: %d", signal);
        Logger::Shutdown();
        ExitProcess(253);
    }

    class ParentMonitor {
    public:
        explicit ParentMonitor(DWORD pid) {
            if (pid != 0) {
                handle_ = OpenProcess(SYNCHRONIZE, FALSE, pid);
            }
        }

        ~ParentMonitor() {
            if (handle_) {
                CloseHandle(handle_);
            }
        }

        bool IsExpired() const {
            if (!handle_) {
                return false;
            }
            return WaitForSingleObject(handle_, 0) == WAIT_OBJECT_0;
        }

    private:
        HANDLE handle_ = nullptr;
    };

    class ScopedMutex {
    public:
        explicit ScopedMutex(const std::string& name) {
            handle_ = CreateMutexA(nullptr, TRUE, name.c_str());
            alreadyExists_ = handle_ && GetLastError() == ERROR_ALREADY_EXISTS;
        }

        ~ScopedMutex() {
            if (handle_) {
                if (!alreadyExists_) {
                    ReleaseMutex(handle_);
                }
                CloseHandle(handle_);
            }
        }

        bool IsValid() const { return handle_ != nullptr; }
        bool AlreadyExists() const { return alreadyExists_; }

    private:
        HANDLE handle_ = nullptr;
        bool alreadyExists_ = false;
    };

    class ArchipelagoSession {
    public:
        ArchipelagoSession(const ApConfig::Settings& cfg,
                           std::vector<std::string>& receivedItems,
                           std::set<std::string>& receivedItemSet,
                           std::set<std::string>& sentLocations)
            : cfg_(cfg)
            , uri_(NormalizeServerUri(cfg.server))
            , certBundlePath_(ResolveCaBundlePath())
            , receivedItems_(receivedItems)
            , receivedItemSet_(receivedItemSet)
            , sentLocations_(sentLocations)
            , client_(std::make_unique<APClient>(BuildUuid(), kGameName, uri_, certBundlePath_)) {
            storageKey_ = std::string(kRunStateStorageKeyPrefix) + cfg_.slot;
            Logger::Log("AP session config: server='%s' uri='%s' cert='%s'",
                        cfg_.server.c_str(),
                        uri_.c_str(),
                        certBundlePath_.empty() ? "<system-store>" : certBundlePath_.c_str());
            client_->set_socket_connected_handler([this]() {
                Logger::Log("APClient socket connected");
            });
            client_->set_socket_error_handler([this](const std::string& message) {
                Logger::Log("APClient socket error: %s", message.c_str());
                slotConnected_ = false;
                connectedAnnounced_ = false;
                WriteStatus(false, cfg_.slot, cfg_.server);
            });
            client_->set_socket_disconnected_handler([this]() {
                Logger::Log("APClient socket disconnected");
                slotConnected_ = false;
                connectedAnnounced_ = false;
                WriteStatus(false, cfg_.slot, cfg_.server);
            });
            client_->set_room_info_handler([this]() {
                Logger::Log("APClient room info received; connecting slot '%s'",
                            cfg_.slot.c_str());
                // Room-change guard: reset stale local state BEFORE anything
                // can be pushed into the fresh room.
                const std::string seed = client_->get_seed();
                if (!seed.empty()) {
                    const std::string last = JsonLite::ReadScalarString(
                        ReadTextFile(PluginPaths::InGameDir(kRoomFilename)),
                        "seed_name", "");
                    if (seed != last) {
                        ResetLocalStateForNewRoom(seed, cfg_.slot);
                        receivedItems_.clear();
                        receivedItemSet_.clear();
                        lastPushedState_.clear();
                    }
                }
                if (!client_->ConnectSlot(cfg_.slot, cfg_.password, 7)) {
                    Logger::Log("APClient ConnectSlot failed");
                }
            });
            client_->set_slot_connected_handler([this](const nlohmann::json&) {
                slotConnected_ = true;
                SyncServerCheckedLocations(client_->get_checked_locations(), "connected");
                Logger::Log("Authenticated as slot='%s' server='%s'",
                            cfg_.slot.c_str(), cfg_.server.c_str());
                // Pull the run state stored in this room's data storage before
                // pushing anything: at connect, the server copy wins.
                if (!client_->Get({storageKey_})) {
                    Logger::Log("Data storage Get failed for '%s'", storageKey_.c_str());
                    storageReady_ = true;  // fall back to push-only sync
                    MarkStorageSettled();
                    AnnounceConnected();
                }
            });
            client_->set_retrieved_handler(
                [this](const std::map<std::string, nlohmann::json>& keys) {
                    const auto it = keys.find(storageKey_);
                    if (it == keys.end()) {
                        return;
                    }
                    // Server/DataStorage is authoritative at connect. The
                    // local INI is only a live ASI cache; a newer local mtime
                    // must not resurrect stale position/cash or skip GML.
                    std::string serverState;
                    if (it->second.is_object()) {
                        serverState = it->second.value("ini", std::string());
                    } else if (it->second.is_string()) {
                        serverState = it->second.get<std::string>(); // legacy
                    }
                    if (!serverState.empty()) {
                        serverState = SanitizeRunStateIni(serverState);
                    }
                    const std::string iniPath =
                        PluginPaths::InGameDir(kRunStateIniFilename);
                    const std::string localState = ReadSanitizedRunStateIni(iniPath);
                    if (!serverState.empty()) {
                        if (serverState != localState) {
                            WriteTextFile(iniPath, serverState);
                            BumpStateSyncSeq();
                            Logger::Log("Run state downloaded from data storage "
                                        "(%zu bytes, server authoritative, key='%s')",
                                        serverState.size(), storageKey_.c_str());
                        } else {
                            MarkStorageSettled();
                        }
                        lastPushedState_ = serverState;
                    } else {
                        if (!localState.empty() && DeleteFileA(iniPath.c_str())) {
                            BumpStateSyncSeq();
                            lastPushedState_.clear();
                            Logger::Log("No run state in data storage; local run state cleared");
                        } else {
                            MarkStorageSettled();
                        }
                        Logger::Log("No run state in data storage yet (key='%s')",
                                    storageKey_.c_str());
                    }
                    storageReady_ = true;
                    AnnounceConnected();
                });
            client_->set_slot_refused_handler([this](const std::list<std::string>& reasons) {
                slotConnected_ = false;
                connectedAnnounced_ = false;
                std::string reasonText;
                for (const std::string& reason : reasons) {
                    if (!reasonText.empty()) {
                        reasonText += "; ";
                    }
                    reasonText += reason;
                }
                if (reasonText.empty()) {
                    reasonText = "slot refused";
                }
                Logger::Log("APClient slot refused: %s", reasonText.c_str());
                Toast(reasonText);
                WriteStatus(false, cfg_.slot, cfg_.server);
            });
            client_->set_items_received_handler(
                [this](const std::list<APClient::NetworkItem>& items) {
                    bool changed = false;
                    for (const APClient::NetworkItem& item : items) {
                        const char* itemName = ApData::LookupItemName(
                            static_cast<int>(item.item));
                        if (!itemName) {
                            continue;
                        }
                        if (receivedItemSet_.insert(itemName).second) {
                            receivedItems_.push_back(itemName);
                            changed = true;
                            Logger::Log("Received item: %s", itemName);
                        }
                    }
                    if (changed) {
                        WriteItems(receivedItems_);
                    }
                });
            client_->set_location_checked_handler(
                [this](const std::list<int64_t>&) {
                    SyncServerCheckedLocations(client_->get_checked_locations(), "server update");
                });
            client_->set_print_json_handler([this](const APClient::PrintJSONArgs& args) {
                std::string message;
                if (args.message) {
                    message = *args.message;
                }
                if (message.empty()) {
                    for (const APClient::TextNode& node : args.data) {
                        message += node.text;
                    }
                }
                message = Trim(message);
                if (message.empty()) {
                    return;
                }
                Logger::Log("Server message [%s]: %s", args.type.c_str(), message.c_str());
                if (args.type.empty() || args.type == "Chat" || args.type == "ServerChat") {
                    if (message.size() > 80) {
                        message.resize(80);
                    }
                    Toast(message);
                }
            });
        }

        void Poll() {
            client_->poll();
        }

        void SyncServerCheckedLocations(const std::set<int64_t>& checkedIds,
                                        const char* reason) {
            const std::set<std::string> checkedNames =
                WriteServerCheckedLocations(checkedIds, cfg_.slot, client_->get_seed());
            sentLocations_.clear();
            sentLocations_.insert(checkedNames.begin(), checkedNames.end());
            Logger::Log("Synced %zu checked location(s) from server (%s)",
                        checkedNames.size(),
                        reason ? reason : "unspecified");
        }

        void AnnounceConnected() {
            if (connectedAnnounced_) {
                return;
            }
            connectedAnnounced_ = true;
            WriteStatus(true, cfg_.slot, cfg_.server);
            Toast("AP connected");
        }

        bool IsReadyForChecks() const {
            return slotConnected_;
        }

        bool QueueChecks(const std::vector<int>& ids) {
            if (ids.empty() || !slotConnected_) {
                return true;
            }
            std::list<int64_t> locations;
            for (int id : ids) {
                locations.push_back(id);
            }
            return client_->LocationChecks(locations);
        }

        // Upload the local run state when it changed. Gated on storageReady_ so
        // the connect-time download always lands before the first push (a fresh
        // local file must not clobber a server-side run).
        void PushRunState() {
            if (!slotConnected_ || !storageReady_) {
                return;
            }
            const std::string iniPath = PluginPaths::InGameDir(kRunStateIniFilename);
            const std::string state = ReadSanitizedRunStateIni(iniPath);
            if (state.empty() || state == lastPushedState_) {
                return;
            }
            struct _stat64 st {};
            const double mtime = _stat64(iniPath.c_str(), &st) == 0
                ? static_cast<double>(st.st_mtime) : 0.0;
            APClient::DataStorageOperation replace;
            replace.operation = "replace";
            replace.value = BuildRunStatePayload(state, mtime);
            if (client_->Set(storageKey_, nlohmann::json::object(), false, {replace})) {
                lastPushedState_ = state;
                Logger::Log("Run state pushed to data storage (%zu bytes)",
                            state.size());
            }
        }

    private:
        ApConfig::Settings               cfg_;
        std::string                      uri_;
        std::string                      certBundlePath_;
        std::vector<std::string>&        receivedItems_;
        std::set<std::string>&           receivedItemSet_;
        std::set<std::string>&           sentLocations_;
        std::unique_ptr<APClient>        client_;
        bool                             slotConnected_ = false;
        bool                             storageReady_ = false;
        bool                             connectedAnnounced_ = false;
        std::string                      storageKey_ = std::string(kRunStateStorageKeyPrefix) + "default";
        std::string                      lastPushedState_;
    };

    class BridgeService {
    public:
        BridgeService()
            : statePath_(PluginPaths::InGameDir(kStateFilename)) {
        }

        int Run(const ParentMonitor& parent) {
            std::string lastSessionKey;
            std::string lastLoggedConfigKey;
            bool warnedUnsupported = false;
            while (!parent.IsExpired()) {
                const ApConfig::Settings cfg = ApConfig::LoadFromGameDir();
                const std::string configKey =
                    std::to_string(cfg.autoConnect ? 1 : 0) + "\n" + cfg.server + "\n" + cfg.slot;
                if (configKey != lastLoggedConfigKey) {
                    Logger::Log("Bridge loop: autoconnect=%d server='%s' slot='%s'",
                                cfg.autoConnect ? 1 : 0, cfg.server.c_str(), cfg.slot.c_str());
                    lastLoggedConfigKey = configKey;
                }
                if (!cfg.autoConnect) {
                    Logger::Log("Autoconnect disabled in AP_connexion.txt; bridge exits");
                    pythonBridge_.Stop();
                    WriteStatus(false, cfg.slot, cfg.server);
                    return 0;
                }

                if (ShouldUsePythonBridge(cfg.server)) {
                    if (session_) {
                        session_.reset();
                        sentLocations_.clear();
                        lastSessionKey.clear();
                    }
                    if (!pythonBridge_.EnsureRunning(cfg)) {
                        WriteStatus(false, cfg.slot, cfg.server);
                    }
                    Sleep(250);
                    continue;
                }

                pythonBridge_.Stop();

                if (!BackendSupportsServer(cfg.server)) {
                    if (!warnedUnsupported) {
                        Logger::Log("Current GTA3_AP build does not include SSL support; "
                                    "configure AP_connexion.txt with localhost:38281 for testing");
                        warnedUnsupported = true;
                    }
                    WriteStatus(false, cfg.slot, cfg.server);
                    Sleep(500);
                    continue;
                }
                warnedUnsupported = false;

                const std::string sessionKey = cfg.server + "\n" + cfg.slot + "\n" + cfg.password;
                if (!session_ || sessionKey != lastSessionKey) {
                    session_ = std::make_unique<ArchipelagoSession>(
                        cfg, receivedItems_, receivedItemSet_, sentLocations_);
                    lastSessionKey = sessionKey;
                    sentLocations_.clear();
                    WriteStatus(false, cfg.slot, cfg.server);
                    Logger::Log("Created APClient session for server='%s' slot='%s'",
                                cfg.server.c_str(), cfg.slot.c_str());
                }

                try {
                    session_->Poll();
                    PushChecks();
                    // Run-state upload, throttled: the loop sleeps 100 ms, so
                    // every 20th iteration is ~2 s — fast enough for the
                    // plugin's own ~1.5 s mirror cadence.
                    if (++runStatePushTick_ >= 20) {
                        runStatePushTick_ = 0;
                        session_->PushRunState();
                    }
                } catch (const std::exception& ex) {
                    Logger::Log("Bridge loop exception: %s", ex.what());
                    WriteStatus(false, cfg.slot, cfg.server);
                    session_.reset();
                    lastSessionKey.clear();
                    Sleep(1000);
                    continue;
                } catch (...) {
                    Logger::Log("Bridge loop exception: unknown");
                    WriteStatus(false, cfg.slot, cfg.server);
                    session_.reset();
                    lastSessionKey.clear();
                    Sleep(1000);
                    continue;
                }
                Sleep(100);
            }

            Logger::Log("Parent process ended; bridge exits");
            return 0;
        }

    private:
        bool PushChecks() {
            if (!session_ || !session_->IsReadyForChecks()) {
                return true;
            }
            const std::string state = ReadTextFile(statePath_);
            if (state.empty()) {
                return true;
            }
            std::vector<std::string> locationNames;
            std::vector<int> newIds;
            JsonLite::ReadStringArray(state, "locations_checked", [this, &locationNames, &newIds](
                                                                   const std::string& locName) {
                if (sentLocations_.count(locName) != 0) {
                    return;
                }
                int id = 0;
                if (!ApData::LookupLocationId(locName, &id)) {
                    return;
                }
                locationNames.push_back(locName);
                newIds.push_back(id);
            });
            if (newIds.empty()) {
                return true;
            }
            if (!session_->QueueChecks(newIds)) {
                return false;
            }
            for (const std::string& locName : locationNames) {
                sentLocations_.insert(locName);
            }
            Logger::Log("Queued %zu location check(s)", newIds.size());
            return true;
        }
        std::string             statePath_;
        int                     runStatePushTick_ = 0;
        std::vector<std::string> receivedItems_;
        std::set<std::string>   receivedItemSet_;
        std::set<std::string>   sentLocations_;
        PythonBridgeWorker      pythonBridge_;
        std::unique_ptr<ArchipelagoSession> session_;
    };

    DWORD ParseParentPid(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            if (_strnicmp(argv[i], "--parent-pid=", 13) == 0) {
                return static_cast<DWORD>(std::strtoul(argv[i] + 13, nullptr, 10));
            }
        }
        return 0;
    }

    std::string ParseGameDirOverride(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            if (_strnicmp(argv[i], "--game-dir=", 11) == 0) {
                return argv[i] + 11;
            }
        }
        return std::string();
    }
}

int main(int argc, char** argv) {
    int rc = 1;
    char exePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        exePath[length] = '\0';
        PluginPaths::InitFromPath(exePath);
    } else {
        PluginPaths::InitFromPath(argv[0]);
    }

    const std::string gameDirOverride = ParseGameDirOverride(argc, argv);
    if (!gameDirOverride.empty()) {
        PluginPaths::OverrideGameDir(gameDirOverride.c_str());
    }

    Logger::Init(PluginPaths::InGameDir(kBridgeLogFilename));
    Logger::Log("%s starting (gameDir=%s)", kBridgeExeName, PluginPaths::InGameDir("").c_str());
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        Logger::Log("SymInitialize failed (err=%lu)", GetLastError());
    }
    SetUnhandledExceptionFilter(&TopLevelExceptionFilter);
    std::set_terminate(&TerminateHandler);
    _set_purecall_handler(&PurecallHandler);
    _set_invalid_parameter_handler(&InvalidParameterHandler);
    signal(SIGABRT, &AbortSignalHandler);
    FILE* redirected = nullptr;
    freopen_s(&redirected, PluginPaths::InGameDir(kBridgeStdoutFilename).c_str(), "a", stdout);
    if (redirected) {
        setvbuf(stdout, nullptr, _IONBF, 0);
    }
    redirected = nullptr;
    freopen_s(&redirected, PluginPaths::InGameDir(kBridgeStdoutFilename).c_str(), "a", stderr);
    if (redirected) {
        setvbuf(stderr, nullptr, _IONBF, 0);
    }

    ScopedMutex instanceMutex(
        ApBridgeProcess::InstanceMutexNameForGameDir(PluginPaths::InGameDir("")));
    if (!instanceMutex.IsValid()) {
        Logger::Log("Failed to create bridge mutex (err=%lu)", GetLastError());
        Logger::Shutdown();
        return 1;
    }
    if (instanceMutex.AlreadyExists()) {
        Logger::Log("Bridge already running for this game directory");
        Logger::Shutdown();
        return 0;
    }

    const ParentMonitor parent(ParseParentPid(argc, argv));
    try {
        BridgeService service;
        rc = service.Run(parent);
    } catch (const std::exception& ex) {
        Logger::Log("Fatal exception: %s", ex.what());
        rc = 2;
    } catch (...) {
        Logger::Log("Fatal exception: unknown");
        rc = 3;
    }
    SymCleanup(GetCurrentProcess());
    Logger::Shutdown();
    return rc;
}
