#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

enum class ApiUrlStatus
{
    Empty,
    Invalid,
    Insecure,
    Secure
};

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

    [[nodiscard]] ApiUrlStatus urlStatus() const
    {
        const auto url = normalizedApiUrl();
        if (url.empty())
            return ApiUrlStatus::Empty;

        if (hasSchemeAndHost(url, "https://"))
            return ApiUrlStatus::Secure;

        if (hasSchemeAndHost(url, "http://"))
            return ApiUrlStatus::Insecure;

        return ApiUrlStatus::Invalid;
    }

    [[nodiscard]] bool isSecure() const
    {
        return urlStatus() == ApiUrlStatus::Secure;
    }

private:
    static bool hasSchemeAndHost(const std::string_view url, const std::string_view scheme)
    {
        if (url.size() <= scheme.size())
            return false;

        return std::ranges::equal(
            scheme,
            url.substr(0, scheme.size()),
            [](const char expected, const char actual)
            {
                return expected == static_cast<char>(std::tolower(static_cast<unsigned char>(actual)));
            });
    }
};
