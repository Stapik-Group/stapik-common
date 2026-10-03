#include "LocaleManager.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/locale/SystemLanguage.hpp"
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

namespace
{
    LocalizationEngine createEngine()
    {
        const auto appLocales = AppPaths::resourcesDir() / "locales";
        const auto commonLocales = AppPaths::commonResourcesDir() / "locales";

        if (std::error_code errorCode; !std::filesystem::is_directory(commonLocales, errorCode))
            return LocalizationEngine(appLocales);

        LocalizationEngine engine(commonLocales);
        engine.addLocalesDirectory(appLocales);
        return engine;
    }

    std::string systemDefaultLanguage(const LocalizationEngine& engine)
    {
        std::vector<std::string> available;
        for (const auto& language : engine.languages())
            available.push_back(language.code);

        return stapik::locale::systemLanguageCode(available, "en");
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
    m_engine(createEngine()),
    m_languageCode(localeSettingsStore(appName), "locale", systemDefaultLanguage(m_engine))
{
    m_engine.setLanguage(m_languageCode.get());
}

void LocaleManager::setLocale(const Locale locale)
{
    setLanguage(toFileString(locale));
}

Locale LocaleManager::getLocale() const
{
    return fromFileString(m_engine.languageCode());
}

void LocaleManager::setLanguage(const std::string& languageCode)
{
    m_languageCode.set(languageCode);
    m_engine.setLanguage(languageCode);
    m_signalLocaleChanged.emit();
}

const std::string& LocaleManager::languageCode() const
{
    return m_engine.languageCode();
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
