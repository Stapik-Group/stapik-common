#pragma once

#include "Semver.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace stapik::update
{
    class UpdateCheckException : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    struct ReleaseInfo
    {
        std::string tag;
        std::string name;
        std::string pageUrl;
        Semver version;
    };

    // "https://github.com/Owner/Repo(.git)" -> "Owner/Repo"; empty for anything that is not a GitHub repository.
    [[nodiscard]] std::optional<std::string> githubRepositoryOf(std::string_view repositoryUrl);

    [[nodiscard]] std::string latestReleaseApiUrl(std::string_view repository);

    // Throws UpdateCheckException when the body is not a release with a semantic version tag and an https page.
    [[nodiscard]] ReleaseInfo parseLatestRelease(const std::string& body);
}
