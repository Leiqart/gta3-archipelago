#pragma once

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace JsonLite {
    inline std::size_t FindValue(const std::string& s, const std::string& key) {
        const std::string pat = "\"" + key + "\"";
        std::size_t p = s.find(pat);
        if (p == std::string::npos) {
            return std::string::npos;
        }
        p += pat.size();
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
        if (p >= s.size() || s[p] != ':') {
            return std::string::npos;
        }
        ++p;
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
        return p;
    }

    inline bool ReadString(const std::string& s, std::size_t& pos, std::string& out) {
        if (pos >= s.size() || s[pos] != '"') {
            return false;
        }
        ++pos;
        out.clear();
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                ++pos;
                const char c = s[pos];
                out.push_back(c == 'n' ? '\n' : c == 't' ? '\t' : c);
            } else {
                out.push_back(s[pos]);
            }
            ++pos;
        }
        if (pos < s.size() && s[pos] == '"') {
            ++pos;
            return true;
        }
        return false;
    }

    inline std::string ReadScalarString(const std::string& s, const std::string& key,
                                        const std::string& def) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos) {
            return def;
        }
        std::string v;
        return ReadString(s, p, v) ? v : def;
    }

    inline bool ReadBool(const std::string& s, const std::string& key, bool def) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos) {
            return def;
        }
        if (s.compare(p, 4, "true") == 0)  return true;
        if (s.compare(p, 5, "false") == 0) return false;
        return def;
    }

    inline int ReadInt(const std::string& s, const std::string& key, int def) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos) {
            return def;
        }
        return std::atoi(s.c_str() + p);
    }

    template <typename Insert>
    inline void ReadStringArray(const std::string& s, const std::string& key, Insert insert) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos || p >= s.size() || s[p] != '[') {
            return;
        }
        ++p;
        while (p < s.size()) {
            while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
            if (p >= s.size() || s[p] == ']') break;
            if (s[p] == '"') {
                std::string v;
                if (ReadString(s, p, v)) insert(v);
            } else {
                ++p;
            }
            while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
            if (p < s.size() && s[p] == ',') ++p;
            else break;
        }
    }

    inline bool ExtractArray(const std::string& s, const std::string& key, std::string& out) {
        std::size_t p = FindValue(s, key);
        if (p == std::string::npos || p >= s.size() || s[p] != '[') {
            return false;
        }
        std::size_t start = p;
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (; p < s.size(); ++p) {
            const char c = s[p];
            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
            } else if (c == '[') {
                ++depth;
            } else if (c == ']') {
                --depth;
                if (depth == 0) {
                    out.assign(s, start, p - start + 1);
                    return true;
                }
            }
        }
        return false;
    }

    inline void SplitTopLevelObjects(const std::string& s, std::vector<std::string>& out) {
        out.clear();
        int depth = 0;
        bool inString = false;
        bool escaped = false;
        std::size_t start = std::string::npos;
        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
                continue;
            }
            if (c == '{') {
                if (depth == 0) {
                    start = i;
                }
                ++depth;
                continue;
            }
            if (c == '}') {
                --depth;
                if (depth == 0 && start != std::string::npos) {
                    out.emplace_back(s.substr(start, i - start + 1));
                    start = std::string::npos;
                }
            }
        }
    }

    inline std::string Escape(const std::string& v) {
        std::string out;
        out.reserve(v.size() + 8);
        for (char c : v) {
            if (c == '"' || c == '\\') {
                out.push_back('\\');
            }
            out.push_back(c);
        }
        return out;
    }
}
