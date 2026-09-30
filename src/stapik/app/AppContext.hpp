#pragma once

#include "AppInfo.hpp"

#include "stapik/settings/SettingsStore.hpp"

#include <string>

namespace stapik::app
{
    class AppContext
    {
    public:
        static bool initialize(AppInfo appInfo);
        static void initializeFromLegacyName(const std::string& appName);
        static void resetForTests();

        [[nodiscard]] static bool isInitialized();
        [[nodiscard]] static const AppContext& instance();
        [[nodiscard]] const AppInfo& info() const;
        [[nodiscard]] settings::SettingsStore& settings() const;
    private:
        AppContext(AppInfo appInfo, bool createdFromLegacyName);

        AppInfo m_info;
        bool m_createdFromLegacyName;
    };
}
