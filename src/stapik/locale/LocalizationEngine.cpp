#include "LocalizationEngine.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <glib.h>

LocalizationEngine::LocalizationEngine(std::filesystem::path localesDir) : m_localesDir(std::move(localesDir))
{
    loadTranslations();
}

void LocalizationEngine::loadTranslations()
{
    using enum Locale;

    loadLocaleFile(PL, (m_localesDir / "pl.json").string());
    loadLocaleFile(EN, (m_localesDir / "en.json").string());
    loadLocaleFile(DE, (m_localesDir / "de.json").string());
}

void LocalizationEngine::loadLocaleFile(const Locale locale, const std::string &filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        return;

    try
    {
        for (const auto json = nlohmann::json::parse(file); const auto &[key, value]: json.items())
            m_translations[locale][key] = value.get<std::string>();
    } catch (const nlohmann::json::exception &)
    {
        g_warning("No translations for locale %s, empty map.", toString(locale));
    }
}

void LocalizationEngine::setLocale(const Locale locale)
{
    m_currentLocale = locale;
}

Locale LocalizationEngine::getLocale() const
{
    return m_currentLocale;
}

std::string LocalizationEngine::translate(const std::string &key) const
{
    const auto localeIt = m_translations.find(m_currentLocale);
    if (localeIt == m_translations.end())
        return key;

    const auto keyIt = localeIt->second.find(key);
    if (keyIt == localeIt->second.end())
        return key;

    return keyIt->second;
}