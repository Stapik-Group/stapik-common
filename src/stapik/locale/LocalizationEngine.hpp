#pragma once

#include "Locale.hpp"

#include <filesystem>
#include <map>
#include <string>

class LocalizationEngine
{
public:
    explicit LocalizationEngine(std::filesystem::path localesDir);

    void setLocale(Locale locale);
    [[nodiscard]] Locale getLocale() const;
    [[nodiscard]] std::string translate(const std::string& key) const;
private:
    std::filesystem::path m_localesDir;
    Locale m_currentLocale = Locale::PL;
    std::map<Locale, std::map<std::string, std::string>> m_translations;

    void loadTranslations();
    void loadLocaleFile(Locale locale, const std::string& filePath);
};