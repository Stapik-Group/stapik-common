#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

struct CloudStorageConfig
{
    std::string apiUrl;
    std::string apiKey;

    [[nodiscard]] bool isConfigured() const
    {
        return !apiUrl.empty() && !apiKey.empty();
    }

    [[nodiscard]] std::string normalizedApiUrl() const
    {
        constexpr std::string_view ignoredCharacters = " \t\r\n/";

        const std::string_view view(apiUrl);
        const auto first = view.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos)
            return {};

        const auto last = view.find_last_not_of(ignoredCharacters);
        if (last == std::string_view::npos || last < first)
            return {};

        return std::string(view.substr(first, last - first + 1));
    }

    [[nodiscard]] bool isSecure() const
    {
        constexpr std::string_view requiredPrefix = "https://";

        const auto url = normalizedApiUrl();
        if (url.size() <= requiredPrefix.size())
            return false;

        return std::ranges::equal(
            requiredPrefix,
            std::string_view(url).substr(0, requiredPrefix.size()),
            [](const char expected, const char actual)
            {
                return expected == static_cast<char>(std::tolower(static_cast<unsigned char>(actual)));
            });
    }
};