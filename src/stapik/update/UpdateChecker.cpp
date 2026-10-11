#include "UpdateChecker.hpp"

#include "GitHubReleaseClient.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/sync/Timestamp.hpp"

#include <exception>
#include <utility>

namespace stapik::update
{
    namespace
    {
        constexpr auto LAST_CHECK_SETTING = "update.lastCheck";
        constexpr auto ANNOUNCED_VERSION_SETTING = "update.announcedVersion";
        constexpr auto FALLBACK_USER_AGENT = "stapik-common";

        struct FetchResult
        {
            std::optional<ReleaseInfo> release;
            std::string error;
        };

        template <typename Fetcher>
        FetchResult fetchSafely(Fetcher&& fetcher)
        {
            try
            {
                return { .release = std::forward<Fetcher>(fetcher)(), .error = {} };
            }
            catch (const std::exception& exception)
            {
                return { .release = std::nullopt, .error = exception.what() };
            }
        }

        std::string userAgentOf(const app::AppInfo& appInfo)
        {
            if (appInfo.internalName.empty())
                return FALLBACK_USER_AGENT;

            return appInfo.version.empty() ? appInfo.internalName : appInfo.internalName + '/' + appInfo.version;
        }
    }

    UpdateChecker::UpdateChecker(Semver currentVersion, ReleaseFetcher fetcher, settings::SettingsStore& settings) :
        m_currentVersion(std::move(currentVersion)),
        m_fetcher(std::move(fetcher)),
        m_settings(settings)
    {}

    std::unique_ptr<UpdateChecker> UpdateChecker::forApp(const app::AppInfo& appInfo, settings::SettingsStore& settings)
    {
        const auto repository = githubRepositoryOf(appInfo.repositoryUrl);
        if (!repository)
        {
            log::debug("Update check skipped: '{}' is not a GitHub repository URL", appInfo.repositoryUrl);
            return nullptr;
        }

        const auto version = parseSemver(appInfo.version);
        if (!version)
        {
            log::warning("Update check skipped: application version '{}' is not a semantic version", appInfo.version);
            return nullptr;
        }

        return std::make_unique<UpdateChecker>(
            *version,
            [repository = *repository, userAgent = userAgentOf(appInfo)] { return fetchLatestRelease(repository, userAgent); },
            settings);
    }

    sigc::signal<void(const ReleaseInfo&)>& UpdateChecker::signalUpdateAvailable()
    {
        return m_signalUpdateAvailable;
    }

    void UpdateChecker::check()
    {
        if (m_inFlight)
            return;

        if (!m_settings.get<bool>(CHECK_ENABLED_SETTING, true))
        {
            log::debug("Update check is turned off");
            return;
        }

        if (!isDue())
            return;

        m_inFlight = true;
        m_runner.submit(
            [fetcher = m_fetcher] { return fetchSafely(fetcher); },
            [this](const std::optional<FetchResult> &result)
            {
                if (result)
                    onFetched(result->release, result->error);
                else
                    onFetched(std::nullopt, "unexpected failure");
            });
    }

    bool UpdateChecker::isDue() const
    {
        const auto lastCheck = sync::parseIso8601(m_settings.get<std::string>(LAST_CHECK_SETTING, {}));
        if (!lastCheck)
            return true;

        const auto elapsed = std::chrono::system_clock::now() - *lastCheck;
        // A negative interval means the clock went back since the last check.
        return elapsed < std::chrono::seconds::zero() || elapsed >= CHECK_INTERVAL;
    }

    void UpdateChecker::onFetched(const std::optional<ReleaseInfo>& release, const std::string& error)
    {
        m_inFlight = false;

        // A failed lookup is not recorded, so the next start tries again.
        if (!error.empty())
        {
            log::warning("Update check failed: {}", error);
            return;
        }

        m_settings.set(LAST_CHECK_SETTING, sync::toIso8601(std::chrono::system_clock::now()));

        const bool announce = release
            && release->version > m_currentVersion
            && release->version.toString() != m_settings.get<std::string>(ANNOUNCED_VERSION_SETTING, {});

        if (announce)
            m_settings.set(ANNOUNCED_VERSION_SETTING, release->version.toString());

        if (!m_settings.save())
            log::warning("Cannot persist the update check state");

        if (announce)
        {
            log::info("Version {} is available (current: {})", release->version.toString(), m_currentVersion.toString());
            m_signalUpdateAvailable.emit(*release);
        }
    }
}
