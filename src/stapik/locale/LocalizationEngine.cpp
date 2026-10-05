#include "LocalizationEngine.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/PathText.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace
{
    constexpr auto FALLBACK_LANGUAGE = "en";

    std::string substitute(const std::string_view text, const LocalizationEngine::Arguments &arguments)
    {
        if (arguments.empty())
            return std::string(text);

        std::string result;
        result.reserve(text.size());

        std::size_t position = 0;
        while (position < text.size())
        {
            const auto open = text.find('{', position);
            if (open == std::string_view::npos)
            {
                result.append(text.substr(position));
                return result;
            }

            const auto close = text.find('}', open + 1);
            if (close == std::string_view::npos)
                break;

            result.append(text.substr(position, open - position));

            const auto name = std::string(text.substr(open + 1, close - open - 1));
            if (const auto argument = arguments.find(name); argument != arguments.end())
                result.append(argument->second);
            else
                result.append(text.substr(open, close - open + 1));

            position = close + 1;
        }

        result.append(text.substr(position));
        return result;
    }
}

LocalizationEngine::LocalizationEngine(const std::filesystem::path &localesDir) : m_registry(localesDir)
{
    loadAll();
}

void LocalizationEngine::addLocalesDirectory(const std::filesystem::path &localesDir)
{
    m_registry.addDirectory(localesDir);
    loadAll();
}

void LocalizationEngine::loadAll()
{
    m_translations.clear();

    for (const auto &language: m_registry.languages())
    {
        for (const auto &file: language.files)
            loadFile(language.code, file);
    }
}

void LocalizationEngine::loadFile(const std::string &code, const std::filesystem::path &file)
{
    std::ifstream stream(file);
    if (!stream.is_open())
    {
        stapik::log::warning("Cannot open translation file {}", stapik::storage::pathText(file));
        return;
    }

    try
    {
        const auto json = nlohmann::json::parse(stream);
        if (!json.is_object())
        {
            stapik::log::warning("Translation file {} must contain a JSON object", stapik::storage::pathText(file));
            return;
        }

        for (const auto &[key, value]: json.items())
        {
            if (!value.is_string())
            {
                stapik::log::warning("Ignoring non-string translation '{}' in {}", key, stapik::storage::pathText(file));
                continue;
            }

            m_translations[code][key] = value.get<std::string>();
        }
    } catch (const nlohmann::json::exception &exception)
    {
        stapik::log::warning("Cannot parse translations from {}: {}", stapik::storage::pathText(file), exception.what());
    }
}

void LocalizationEngine::setLocale(const Locale locale)
{
    setLanguage(toFileString(locale));
}

Locale LocalizationEngine::getLocale() const
{
    return fromFileString(m_languageCode);
}

void LocalizationEngine::setLanguage(const std::string_view code)
{
    m_languageCode = std::string(code);
}

const std::string &LocalizationEngine::languageCode() const
{
    return m_languageCode;
}

const std::vector<stapik::locale::LanguageInfo> &LocalizationEngine::languages() const
{
    return m_registry.languages();
}

const std::string *LocalizationEngine::find(const std::string_view code, const std::string_view key) const
{
    const auto language = m_translations.find(code);
    if (language == m_translations.end())
        return nullptr;

    const auto translation = language->second.find(key);
    return translation == language->second.end() ? nullptr : &translation->second;
}

std::string LocalizationEngine::translate(const std::string_view key) const
{
    return translate(key, Arguments{});
}

std::string LocalizationEngine::translate(const std::string_view key, const Arguments &arguments) const
{
    if (const auto *text = find(m_languageCode, key))
        return substitute(*text, arguments);

    if (const auto *text = find(FALLBACK_LANGUAGE, key))
        return substitute(*text, arguments);

    return std::string(key);
}
