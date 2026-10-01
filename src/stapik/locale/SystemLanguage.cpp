#include "SystemLanguage.hpp"

#include <glib.h>

#include <algorithm>
#include <cctype>

namespace stapik::locale
{
    namespace
    {
        // "pl_PL.UTF-8" -> "pl", "en-US" -> "en", "de@euro" -> "de".
        std::string languagePart(const std::string& name)
        {
            std::string language = name.substr(0, name.find_first_of(".@_-"));

            std::ranges::transform(language, language.begin(), [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

            return language;
        }
    }

    std::string matchLanguage(
        const std::vector<std::string>& preferred,
        const std::vector<std::string>& available,
        const std::string_view fallback)
    {
        for (const auto& name : preferred)
        {
            const auto language = languagePart(name);
            if (language.empty() || language == "c" || language == "posix")
                continue;

            if (std::ranges::find(available, language) != available.end())
                return language;
        }

        return std::string(fallback);
    }

    std::string systemLanguageCode(const std::vector<std::string>& available, const std::string_view fallback)
    {
        std::vector<std::string> preferred;
        for (const gchar* const* name = g_get_language_names(); name != nullptr && *name != nullptr; ++name)
            preferred.emplace_back(*name);

        return matchLanguage(preferred, available, fallback);
    }
}
