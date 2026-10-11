#include "GitHubRelease.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace stapik::update
{
    namespace
    {
        constexpr std::string_view GITHUB_URL_PREFIX = "https://github.com/";
        constexpr std::string_view GIT_SUFFIX = ".git";
        constexpr std::string_view HTTPS_PREFIX = "https://";

        bool isNameCharacter(const char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_' || c == '.';
        }

        bool isValidName(const std::string_view name)
        {
            return !name.empty() && name != "." && name != ".." && std::ranges::all_of(name, isNameCharacter);
        }

        bool startsWithIgnoringCase(const std::string_view text, const std::string_view prefix)
        {
            return text.size() >= prefix.size()
                && std::ranges::equal(prefix, text.substr(0, prefix.size()), [](const char expected, const char actual)
                {
                    return expected == static_cast<char>(std::tolower(static_cast<unsigned char>(actual)));
                });
        }

        std::string optionalString(const nlohmann::json& json, const char* key)
        {
            const auto entry = json.find(key);
            return entry != json.end() && entry->is_string() ? entry->get<std::string>() : std::string();
        }
    }

    std::optional<std::string> githubRepositoryOf(std::string_view repositoryUrl)
    {
        if (!startsWithIgnoringCase(repositoryUrl, GITHUB_URL_PREFIX))
            return std::nullopt;

        repositoryUrl.remove_prefix(GITHUB_URL_PREFIX.size());

        const auto ownerEnd = repositoryUrl.find('/');
        if (ownerEnd == std::string_view::npos)
            return std::nullopt;

        const auto owner = repositoryUrl.substr(0, ownerEnd);
        auto repository = repositoryUrl.substr(ownerEnd + 1);
        repository = repository.substr(0, repository.find_first_of("/?#"));
        if (repository.ends_with(GIT_SUFFIX))
            repository.remove_suffix(GIT_SUFFIX.size());

        if (!isValidName(owner) || !isValidName(repository))
            return std::nullopt;

        return std::string(owner) + '/' + std::string(repository);
    }

    std::string latestReleaseApiUrl(const std::string_view repository)
    {
        return "https://api.github.com/repos/" + std::string(repository) + "/releases/latest";
    }

    ReleaseInfo parseLatestRelease(const std::string& body)
    {
        try
        {
            const auto json = nlohmann::json::parse(body);

            ReleaseInfo release;
            release.tag = json.at("tag_name").get<std::string>();
            release.pageUrl = json.at("html_url").get<std::string>();
            release.name = optionalString(json, "name");

            const auto version = parseSemver(release.tag);
            if (!version)
                throw UpdateCheckException("Release tag '" + release.tag + "' is not a semantic version");

            // The URL is handed to the system browser, so never accept other schemes from a remote response.
            if (!startsWithIgnoringCase(release.pageUrl, HTTPS_PREFIX))
                throw UpdateCheckException("Release page '" + release.pageUrl + "' is not an https URL");

            release.version = *version;
            return release;
        }
        catch (const nlohmann::json::exception& exception)
        {
            throw UpdateCheckException(std::string("Failed to parse release response: ") + exception.what());
        }
    }
}
