#include <windows.h>
#include <deque>
#include <string>
#include "plugin.h"
#include "CMessages.h"
#include "Message.h"

namespace {
    // The game keeps only a pointer to a message while it shows it, so the texts stay here; it never queues more than 8.
    std::deque<std::wstring> g_texts;
    constexpr std::size_t kKeptTexts = 16;

    // The text in the wide characters the game draws, read as UTF-8.
    std::wstring Wide(const std::string& text) {
        std::wstring wide(text.size(), L'\0');
        wide.resize(MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), static_cast<int>(wide.size())));
        return wide;
    }
}

void Message::Show(const std::string& text) {
    g_texts.push_back(Wide(text));
    if (g_texts.size() > kKeptTexts) g_texts.pop_front();
    CMessages::AddMessage(g_texts.back().c_str(), 3000, 0);
}
