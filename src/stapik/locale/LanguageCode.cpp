#include "LanguageCode.hpp"

#include <cctype>

namespace stapik::locale
{
    std::string primaryLanguage(const std::string_view languageCode)
    {
        std::string language;
        for (const char character : languageCode.substr(0, languageCode.find_first_of("_-.@")))
            language.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        return language;
    }
}
