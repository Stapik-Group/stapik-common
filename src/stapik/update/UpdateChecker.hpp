#pragma once

#include "GitHubRelease.hpp"

#include "stapik/app/AppInfo.hpp"
#include "stapik/settings/SettingsStore.hpp"
#include "stapik/task/BackgroundTaskRunner.hpp"

#include <sigc++/signal.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace stapik::update
{
    inline constexpr auto CHECK_ENABLED_SETTING = "update.checkEnabled";

    class UpdateChecker
    {
    public:
        using ReleaseFetcher = std::function<std::optional<ReleaseInfo>()>;
        static constexpr std::chrono::hours CHECK_INTERVAL{ 24 };
        UpdateChecker(Semver currentVersion, ReleaseFetcher fetcher, settings::SettingsStore& settings);
        [[nodiscard]] static std::unique_ptr<UpdateChecker> forApp(const app::AppInfo& appInfo, settings::SettingsStore& settings);
        void check();
        [[nodiscard]] sigc::signal<void(const ReleaseInfo&)>& signalUpdateAvailable();
    private:
        [[nodiscard]] bool isDue() const;
        void onFetched(const std::optional<ReleaseInfo>& release, const std::string& error);

        Semver m_currentVersion;
        ReleaseFetcher m_fetcher;
        settings::SettingsStore& m_settings;
        sigc::signal<void(const ReleaseInfo&)> m_signalUpdateAvailable;
        bool m_inFlight = false;
        task::BackgroundTaskRunner m_runner;
    };
}
