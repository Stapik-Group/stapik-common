#include "YearMonthRange.hpp"

#include <format>
#include <stdexcept>

namespace stapik::domain
{
    YearMonthRange::Iterator::Iterator(const YearMonth& first, const int offset) :
        m_first(first),
        m_offset(offset)
    {
    }

    YearMonth YearMonthRange::Iterator::operator*() const
    {
        return m_first.addMonths(m_offset);
    }

    YearMonthRange::Iterator& YearMonthRange::Iterator::operator++()
    {
        ++m_offset;
        return *this;
    }

    YearMonthRange::Iterator YearMonthRange::Iterator::operator++(int)
    {
        const Iterator previous = *this;
        ++m_offset;
        return previous;
    }

    YearMonthRange::YearMonthRange(const YearMonth& first, const YearMonth& last) :
        m_first(first),
        m_last(last)
    {
        if (last < first)
            throw std::invalid_argument(std::format("Reversed month range: {} > {}", first.toKey(), last.toKey()));
    }

    const YearMonth& YearMonthRange::first() const
    {
        return m_first;
    }

    const YearMonth& YearMonthRange::last() const
    {
        return m_last;
    }

    int YearMonthRange::size() const
    {
        return m_first.monthsUntil(m_last) + 1;
    }

    bool YearMonthRange::contains(const YearMonth& month) const
    {
        return m_first <= month && month <= m_last;
    }

    YearMonthRange::Iterator YearMonthRange::begin() const
    {
        return {m_first, 0};
    }

    YearMonthRange::Iterator YearMonthRange::end() const
    {
        return {m_first, size()};
    }
}
