#include "DateRange.hpp"

#include <algorithm>
#include <stdexcept>

namespace stapik::domain
{
    namespace
    {
        bool isSupported(const std::chrono::year_month_day& date)
        {
            const auto year = static_cast<int>(date.year());
            return date.ok() && year >= MIN_YEAR_MONTH_YEAR && year <= MAX_YEAR_MONTH_YEAR;
        }
    }

    DateRange::DateRange(const std::chrono::year_month_day& first, const std::chrono::year_month_day& last) :
        m_first(first),
        m_last(last)
    {
        if (!isSupported(first) || !isSupported(last))
            throw std::invalid_argument("Date range contains an unsupported date");

        if (last < first)
            throw std::invalid_argument("Reversed date range");
    }

    DateRange DateRange::ofMonth(const YearMonth& month)
    {
        return {month.firstDay(), month.lastDay()};
    }

    const std::chrono::year_month_day& DateRange::first() const
    {
        return m_first;
    }

    const std::chrono::year_month_day& DateRange::last() const
    {
        return m_last;
    }

    int DateRange::dayCount() const
    {
        const auto days = std::chrono::sys_days{m_last} - std::chrono::sys_days{m_first};
        return static_cast<int>(days.count()) + 1;
    }

    bool DateRange::contains(const std::chrono::year_month_day& date) const
    {
        return m_first <= date && date <= m_last;
    }

    bool DateRange::overlaps(const DateRange& other) const
    {
        return m_first <= other.m_last && other.m_first <= m_last;
    }

    std::optional<DateRange> DateRange::intersection(const DateRange& other) const
    {
        if (!overlaps(other))
            return std::nullopt;

        return DateRange(std::max(m_first, other.m_first), std::min(m_last, other.m_last));
    }

    YearMonthRange DateRange::months() const
    {
        return {YearMonth::fromDate(m_first), YearMonth::fromDate(m_last)};
    }
}
