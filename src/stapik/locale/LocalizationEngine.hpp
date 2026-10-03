#pragma once

#include "Locale.hpp"
#include "LanguageRegistry.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class LocalizationEngine
{
public:
    using Arguments = std::map<std::string, std::string, std::less<>>;

    explicit LocalizationEngine(const std::filesystem::path& localesDir);

    void addLocalesDirectory(const std::filesystem::path& localesDir);
    void setLocale(Locale locale);
    void setLanguage(std::string_view code);
    [[nodiscard]] Locale getLocale() const;
    [[nodiscard]] const std::string& languageCode() const;
    [[nodiscard]] const std::vector<stapik::locale::LanguageInfo>& languages() const;
    [[nodiscard]] std::string translate(std::string_view key) const;
    [[nodiscard]] std::string translate(std::string_view key, const Arguments& arguments) const;
private:
    using Translations = std::map<std::string, std::string, std::less<>>;

    void loadAll();
    void loadFile(const std::string& code, const std::filesystem::path& file);
    [[nodiscard]] const std::string* find(std::string_view code, std::string_view key) const;

    stapik::locale::LanguageRegistry m_registry;
    std::string m_languageCode = "pl";
    std::map<std::string, Translations, std::less<>> m_translations;
};
