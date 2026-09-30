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

    [[nodiscard]] bool isSecure() const
    {
        constexpr std::string_view requiredPrefix = "https://";

        if (apiUrl.size() <= requiredPrefix.size())
            return false;

        return std::ranges::equal(
            requiredPrefix,
            std::string_view(apiUrl).substr(0, requiredPrefix.size()),
            [](const char expected, const char actual)
            {
                return expected == static_cast<char>(std::tolower(static_cast<unsigned char>(actual)));
            });
    }
};