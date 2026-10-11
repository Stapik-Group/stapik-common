#pragma once

#include "DateRange.hpp"
#include "YearMonth.hpp"
#include "YearMonthRange.hpp"

#include <optional>

namespace stapik::domain
{
    // Inclusive range of whole calendar years within the YearMonth year range.
    class YearRange
    {
    public:
        // Throws std::invalid_argument for unsupported years or when last precedes first.
        YearRange(int first, int last);

        [[nodiscard]] int first() const;
        [[nodiscard]] int last() const;
        [[nodiscard]] int yearCount() const;

        [[nodiscard]] bool contains(int year) const;
        [[nodiscard]] bool contains(const YearMonth& month) const;
        [[nodiscard]] bool overlaps(const YearRange& other) const;
        [[nodiscard]] std::optional<YearRange> intersection(const YearRange& other) const;

        [[nodiscard]] YearMonthRange months() const;
        [[nodiscard]] DateRange days() const;

        [[nodiscard]] bool operator==(const YearRange& other) const = default;
    private:
        int m_first;
        int m_last;
    };
}
