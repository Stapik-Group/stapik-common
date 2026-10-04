#pragma once

#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace stapik::sync
{
    [[nodiscard]] inline std::string partitionKeyForYear(const int year)
    {
        return std::format("{:04}", year);
    }

    [[nodiscard]] inline std::optional<int> yearFromPartitionKey(const std::string_view key)
    {
        constexpr std::size_t YEAR_DIGITS = 4;

        if (key.size() != YEAR_DIGITS)
            return std::nullopt;

        int year = 0;
        for (const char c : key)
        {
            if (c < '0' || c > '9')
                return std::nullopt;

            year = year * 10 + (c - '0');
        }
        return year;
    }
}
