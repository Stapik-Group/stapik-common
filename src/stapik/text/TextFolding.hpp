#pragma once

#include <string>
#include <string_view>

namespace stapik::text
{
    [[nodiscard]] constexpr bool isCombiningMark(const char32_t codePoint)
    {
        return codePoint >= 0x0300 && codePoint <= 0x036F;
    }

    [[nodiscard]] std::u32string decompose(std::string_view text);
    [[nodiscard]] std::string toUtf8(std::u32string_view codePoints);
    [[nodiscard]] std::string foldCase(std::string_view text);
    [[nodiscard]] std::string foldForSearch(std::string_view text);
    [[nodiscard]] bool containsFolded(std::string_view text, std::string_view needle);
}
