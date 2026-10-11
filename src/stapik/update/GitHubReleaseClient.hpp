#pragma once

#include "GitHubRelease.hpp"

#include <optional>
#include <string>

namespace stapik::update
{
    // Blocking call. Empty when the repository has no published release; throws UpdateCheckException on failure.
    [[nodiscard]] std::optional<ReleaseInfo> fetchLatestRelease(const std::string& repository, const std::string& userAgent);
}
