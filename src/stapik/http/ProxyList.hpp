#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace stapik::http
{
    [[nodiscard]] std::string_view schemeOf(std::string_view url);
    [[nodiscard]] std::string_view originOf(std::string_view url);

    [[nodiscard]] std::optional<std::string> proxyForScheme(std::string_view proxyList, std::string_view scheme);
    [[nodiscard]] std::string noProxyListOf(std::string_view bypassList);
}
