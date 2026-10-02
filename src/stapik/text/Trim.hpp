#pragma once

#include <string>
#include <string_view>

namespace stapik::text
{
    [[nodiscard]] inline std::string trim(const std::string_view text)
    {
        constexpr std::string_view whitespace = " \t\r\n";

        const auto first = text.find_first_not_of(whitespace);
        if (first == std::string_view::npos)
            return {};

        const auto last = text.find_last_not_of(whitespace);
        return std::string(text.substr(first, last - first + 1));
    }
}
