#include "CurlProxy.hpp"

#include "SystemProxy.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace stapik::http
{
    namespace
    {
        bool hasProxyEnvironment()
        {
            constexpr std::array<const char*, 5> NAMES{ "http_proxy", "https_proxy", "HTTPS_PROXY", "all_proxy", "ALL_PROXY" };

            return std::ranges::any_of(NAMES, [](const char* name)
            {
                const char* value = std::getenv(name);
                return value != nullptr && *value != '\0';
            });
        }
    }

    void applyProxyOptions(CURL* curl, const std::string& url)
    {
        curl_easy_setopt(curl, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);

        if (hasProxyEnvironment())
            return;

        const auto proxy = systemProxyFor(url);
        if (!proxy)
            return;

        curl_easy_setopt(curl, CURLOPT_PROXY, proxy->url.c_str());
        if (!proxy->bypass.empty())
            curl_easy_setopt(curl, CURLOPT_NOPROXY, proxy->bypass.c_str());

#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, ":");
#endif
    }
}
