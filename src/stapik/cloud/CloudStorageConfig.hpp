#pragma once

#include <string>

struct CloudStorageConfig
{
    std::string apiUrl;
    std::string apiKey;

    [[nodiscard]] bool isConfigured() const
    {
        return !apiUrl.empty() && !apiKey.empty();
    }
};