#pragma once

#include <filesystem>
#include <string>

namespace stapik::storage
{
    // UTF-8 text of a path, for log messages, user-visible text and GLib (which expects UTF-8 file names on
    // every platform). path::string() is not good enough: on Windows it converts to the ANSI code page, which
    // loses characters (a Polish or Cyrillic user name) and can throw.
    [[nodiscard]] inline std::string pathText(const std::filesystem::path& path)
    {
        const auto text = path.generic_u8string();
        return {text.begin(), text.end()};
    }

    [[nodiscard]] inline std::filesystem::path pathFromText(const std::string& text)
    {
        std::u8string u8Str(text.begin(), text.end());
        return {u8Str};
    }
}
