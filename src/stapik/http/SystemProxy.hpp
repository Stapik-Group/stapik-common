#pragma once

#include <optional>
#include <string>

namespace stapik::http
{
    struct SystemProxy
    {
        std::string url;
        std::string bypass;
    };

    // Proxy the operating system configured for the URL. Only Windows has such a setting that libcurl does not read
    // itself; elsewhere libcurl follows the *_proxy environment variables and this finds nothing.
    [[nodiscard]] std::optional<SystemProxy> systemProxyFor(const std::string& url);
}
