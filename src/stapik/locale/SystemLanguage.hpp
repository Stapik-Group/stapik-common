#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace stapik::locale
{
    [[nodiscard]] std::string matchLanguage(
        const std::vector<std::string>& preferred,
        const std::vector<std::string>& available,
        std::string_view fallback = "en");

    [[nodiscard]] std::string systemLanguageCode(
        const std::vector<std::string>& available,
        std::string_view fallback = "en");
}
