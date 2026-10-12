#include "YearRange.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace stapik::domain
{
    namespace
    {
        bool isSupportedYear(const int year)
        {
            return year >= MIN_YEAR_MONTH_YEAR && year <= MAX_YEAR_MONTH_YEAR;
        }
    }

    YearRange::YearRange(const int first, const int last) :
        m_first(first),
        m_last(last)
    {
        if (!isSupportedYear(first) || !isSupportedYear(last))
            throw std::invalid_argument(std::format("Unsupported year range: {}..{}", first, last));

        if (last < first)
            throw std::invalid_argument(std::format("Reversed year range: {} > {}", first, last));
    }

    int YearRange::first() const
    {
        return m_first;
    }

    int YearRange::last() const
    {
        return m_last;
    }

    int YearRange::yearCount() const
    {
        return m_last - m_first + 1;
    }

    bool YearRange::contains(const int year) const
    {
        return m_first <= year && year <= m_last;
    }

    bool YearRange::contains(const YearMonth& month) const
    {
        return contains(month.year());
    }

    bool YearRange::overlaps(const YearRange& other) const
    {
        return m_first <= other.m_last && other.m_first <= m_last;
    }

    std::optional<YearRange> YearRange::intersection(const YearRange& other) const
    {
        if (!overlaps(other))
            return std::nullopt;

        return YearRange(std::max(m_first, other.m_first), std::min(m_last, other.m_last));
    }

    YearMonthRange YearRange::months() const
    {
        return {YearMonth(m_first, 1), YearMonth(m_last, MONTHS_PER_YEAR)};
    }

    DateRange YearRange::days() const
    {
        return {YearMonth(m_first, 1).firstDay(), YearMonth(m_last, MONTHS_PER_YEAR).lastDay()};
    }
}
