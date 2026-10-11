#pragma once

#include <string>
#include <string_view>

namespace stapik::locale
{
    // Lower-case language part of a code such as "pt-BR", "pl_PL.UTF-8" or "de@euro".
    [[nodiscard]] std::string primaryLanguage(std::string_view languageCode);
}
