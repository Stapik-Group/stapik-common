#include "LanguageRegistry.hpp"

#include "stapik/log/Log.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>

namespace stapik::locale
{
    namespace
    {
        constexpr auto LANGUAGE_NAME_KEY = "language.name";

        std::string lowercase(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return text;
        }

        std::string uppercase(std::string text)
        {
            std::ranges::transform(text, text.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::toupper(character));
            });
            return text;
        }

        std::optional<std::string> readDisplayName(const std::filesystem::path& file)
        {
            std::ifstream stream(file);
            if (!stream.is_open())
                return std::nullopt;

            try
            {
                if (const auto json = nlohmann::json::parse(stream); json.is_object() && json.contains(LANGUAGE_NAME_KEY) && json.at(LANGUAGE_NAME_KEY).is_string())
                    return json.at(LANGUAGE_NAME_KEY).get<std::string>();
            }
            catch (const nlohmann::json::exception&)
            {
                // Reported when the translations themselves are loaded.
            }

            return std::nullopt;
        }
    }

    LanguageRegistry::LanguageRegistry(const std::filesystem::path& directory)
    {
        addDirectory(directory);
    }

    void LanguageRegistry::addDirectory(const std::filesystem::path& directory)
    {
        std::error_code errorCode;
        if (!std::filesystem::is_directory(directory, errorCode))
        {
            log::warning("Locales directory not found: {}", directory.string());
            return;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(directory, errorCode))
        {
            if (const auto& path = entry.path(); entry.is_regular_file(errorCode) && path.extension() == ".json" && !path.filename().string().starts_with('.'))
                files.push_back(path);
        }

        std::ranges::sort(files);

        for (const auto& file : files)
        {
            const auto code = lowercase(file.stem().string());

            auto language = std::ranges::find(m_languages, code, &LanguageInfo::code);
            if (language == m_languages.end())
            {
                m_languages.push_back(LanguageInfo{ .code = code, .displayName = uppercase(code), .files = {}});
                language = std::prev(m_languages.end());
            }

            language->files.push_back(file);

            if (const auto name = readDisplayName(file))
                language->displayName = *name;
        }

        std::ranges::sort(m_languages, {}, &LanguageInfo::code);
    }

    const std::vector<LanguageInfo>& LanguageRegistry::languages() const
    {
        return m_languages;
    }

    const LanguageInfo* LanguageRegistry::find(const std::string_view code) const
    {
        const auto language = std::ranges::find_if(m_languages, [code](const LanguageInfo& info)
        {
            return info.code == code;
        });

        return language == m_languages.end() ? nullptr : &*language;
    }

    bool LanguageRegistry::contains(const std::string_view code) const
    {
        return find(code) != nullptr;
    }
}
