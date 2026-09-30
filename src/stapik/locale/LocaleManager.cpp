#include "LocaleManager.hpp"

#include "stapik/settings/AppSettings.hpp"
#include "stapik/storage/AppPaths.hpp"

namespace
{
    stapik::settings::SettingsStore& localeSettingsStore(const std::string& appName)
    {
        auto& store = stapik::settings::appSettingsStore(appName);

        stapik::settings::importLegacyTextSetting(
            store,
            "locale",
            AppPaths::userDataDir(appName) / "locale.txt",
            [](const std::string& value) { return std::string(toFileString(fromFileString(value))); });

        return store;
    }
}

LocaleManager& LocaleManager::instance(const std::string& appName)
{
    static LocaleManager manager(appName);
    return manager;
}

LocaleManager::LocaleManager(const std::string& appName) :
    m_engine(AppPaths::resourcesDir() / "locales"),
    m_locale(localeSettingsStore(appName), "locale", Locale::EN)
{
    m_engine.setLocale(m_locale.get());
}

void LocaleManager::setLocale(const Locale locale)
{
    m_locale.set(locale);
    m_engine.setLocale(locale);
    m_signalLocaleChanged.emit();
}

Locale LocaleManager::getLocale() const
{
    return m_engine.getLocale();
}

std::string LocaleManager::translate(const std::string &key) const
{
    return m_engine.translate(key);
}

sigc::signal<void()>& LocaleManager::signalLocaleChanged()
{
    return m_signalLocaleChanged;
}
