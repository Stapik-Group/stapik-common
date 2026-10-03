#pragma once

#include "Money.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace stapik::domain
{
    struct NumberFormat
    {
        std::string decimalSeparator;
        std::string groupSeparator;
    };

    struct MoneyFormatOptions
    {
        bool withSymbol = true;
        bool groupThousands = true;
    };

    [[nodiscard]] NumberFormat numberFormatFor(std::string_view languageCode);

    [[nodiscard]] std::string formatAmount(
        std::int64_t minorUnits,
        int decimalPlaces,
        std::string_view languageCode,
        bool groupThousands = true);

    [[nodiscard]] std::string formatMoney(
        const Money& money,
        std::string_view languageCode,
        MoneyFormatOptions options = {});

    [[nodiscard]] std::optional<std::int64_t> parseAmount(std::string_view text, int decimalPlaces);
}
