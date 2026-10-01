#include "ThemeManager.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/settings/AppSettings.hpp"
#include "stapik/storage/AppPaths.hpp"

namespace
{
    stapik::settings::SettingsStore& themeSettingsStore(const std::string& appName)
    {
        auto& store = stapik::settings::appSettingsStore(appName);

        stapik::settings::importLegacyTextSetting(
            store,
            "theme",
            AppPaths::userDataDir(appName) / "theme.txt",
            [](const std::string& value) { return themeToFileString(themeFromFileString(value)); });

        return store;
    }
}

ThemeManager& ThemeManager::instance()
{
    static ThemeManager manager(stapik::app::AppContext::instance().info().internalName);
    return manager;
}

ThemeManager& ThemeManager::instance(const std::string& appName)
{
    stapik::app::AppContext::initializeFromLegacyName(appName);
    return instance();
}

ThemeManager::ThemeManager(const std::string& appName) :
    m_themeId(themeSettingsStore(appName), "theme", themeToFileString(Theme::Classic))
{}

void ThemeManager::setTheme(const Theme theme)
{
    setThemeId(themeToFileString(theme));
}

Theme ThemeManager::getTheme() const
{
    return themeFromFileString(m_themeId.get());
}

void ThemeManager::setThemeId(const std::string& themeId)
{
    m_themeId.set(themeId);
    m_signalThemeChanged.emit();
}

const std::string& ThemeManager::themeId() const
{
    return m_themeId.get();
}

sigc::signal<void()>& ThemeManager::signalThemeChanged()
{
    return m_signalThemeChanged;
}
