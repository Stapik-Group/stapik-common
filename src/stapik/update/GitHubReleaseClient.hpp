#pragma once

#include "GitHubRelease.hpp"

#include "stapik/http/IHttpClient.hpp"

#include <optional>
#include <string>

namespace stapik::update
{
    // Blocking call. Empty when the repository has no published release; throws UpdateCheckException on failure.
    [[nodiscard]] std::optional<ReleaseInfo> fetchLatestRelease(const std::string& repository, const std::string& userAgent,
                                                                const http::IHttpClient& httpClient = http::defaultHttpClient());
}
