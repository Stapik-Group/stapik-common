#pragma once

#include <cstdint>
#include <string>

namespace stapik::domain
{
    inline constexpr int MAX_DECIMAL_PLACES = 6;

    struct Currency
    {
        std::string code;
        std::string symbol;
        bool symbolBeforeAmount = false;
        int decimalPlaces = 2;

        bool operator==(const Currency&) const = default;

        [[nodiscard]] std::string nameKey() const
        {
            return "currency." + code;
        }

        [[nodiscard]] std::int64_t minorUnitsPerMajor() const
        {
            std::int64_t factor = 1;
            for (int place = 0; place < decimalPlaces; ++place)
                factor *= 10;
            return factor;
        }
    };
}
