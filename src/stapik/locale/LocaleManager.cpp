#include "LocaleManager.hpp"

#include "stapik/app/AppContext.hpp"
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

LocaleManager& LocaleManager::instance()
{
    static LocaleManager manager(stapik::app::AppContext::instance().info().internalName);
    return manager;
}

LocaleManager& LocaleManager::instance(const std::string& appName)
{
    stapik::app::AppContext::initializeFromLegacyName(appName);
    return instance();
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

std::string LocaleManager::translate(const std::string_view key) const
{
    return m_engine.translate(key);
}

std::string LocaleManager::translate(const std::string_view key, const LocalizationEngine::Arguments& arguments) const
{
    return m_engine.translate(key, arguments);
}

const std::vector<stapik::locale::LanguageInfo>& LocaleManager::languages() const
{
    return m_engine.languages();
}

sigc::signal<void()>& LocaleManager::signalLocaleChanged()
{
    return m_signalLocaleChanged;
}
