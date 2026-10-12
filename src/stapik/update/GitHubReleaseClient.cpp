#include "GitHubReleaseClient.hpp"

#include <chrono>
#include <format>

namespace stapik::update
{
    namespace
    {
        constexpr std::chrono::seconds TIMEOUT{ 15 };
        constexpr std::size_t MAX_RESPONSE_BYTES = 2U * 1024U * 1024U;
        constexpr int HTTP_OK = 200;
        constexpr int HTTP_NOT_FOUND = 404;
        constexpr int HTTP_FORBIDDEN = 403;
        constexpr int HTTP_TOO_MANY_REQUESTS = 429;
    }

    std::optional<ReleaseInfo> fetchLatestRelease(const std::string& repository, const std::string& userAgent, const http::IHttpClient& httpClient)
    {
        const http::Request request{
            .url = latestReleaseApiUrl(repository),
            .headers = { { "User-Agent", userAgent }, { "Accept", "application/vnd.github+json" }, { "X-GitHub-Api-Version", "2022-11-28" } },
            .timeout = TIMEOUT,
            .maxResponseBytes = MAX_RESPONSE_BYTES
        };

        const auto result = httpClient.send(request);
        if (!result)
            throw UpdateCheckException("Release lookup failed: " + result.error().message);

        const auto& response = result.value();

        if (response.status == HTTP_NOT_FOUND)
            return std::nullopt;

        if (response.status == HTTP_FORBIDDEN || response.status == HTTP_TOO_MANY_REQUESTS)
            throw UpdateCheckException(std::format("Release lookup was rate limited (status {})", response.status));

        if (response.status != HTTP_OK)
            throw UpdateCheckException(std::format("Release lookup failed with status {}", response.status));

        return parseLatestRelease(http::toText(response.body));
    }
}
