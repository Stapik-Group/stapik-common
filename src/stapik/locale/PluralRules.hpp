#pragma once

#include <string_view>

namespace stapik::locale
{
    enum class PluralCategory
    {
        Zero,
        One,
        Two,
        Few,
        Many,
        Other
    };

    [[nodiscard]] PluralCategory pluralCategory(std::string_view languageCode, long long count);
    [[nodiscard]] std::string_view pluralCategoryName(PluralCategory category);
}
