#pragma once

#include "YearMonth.hpp"
#include "YearMonthRange.hpp"

#include <chrono>
#include <optional>

namespace stapik::domain
{
    // Inclusive range of calendar days; both ends must be valid dates within the YearMonth year range.
    class DateRange
    {
    public:
        // Throws std::invalid_argument for unsupported dates or when last precedes first.
        DateRange(const std::chrono::year_month_day& first, const std::chrono::year_month_day& last);

        [[nodiscard]] static DateRange ofMonth(const YearMonth& month);

        [[nodiscard]] const std::chrono::year_month_day& first() const;
        [[nodiscard]] const std::chrono::year_month_day& last() const;
        [[nodiscard]] int dayCount() const;

        [[nodiscard]] bool contains(const std::chrono::year_month_day& date) const;
        [[nodiscard]] bool overlaps(const DateRange& other) const;
        [[nodiscard]] std::optional<DateRange> intersection(const DateRange& other) const;

        // Every month the range touches, including partially covered ones.
        [[nodiscard]] YearMonthRange months() const;
        [[nodiscard]] bool operator==(const DateRange& other) const = default;
    private:
        std::chrono::year_month_day m_first;
        std::chrono::year_month_day m_last;
    };
}
