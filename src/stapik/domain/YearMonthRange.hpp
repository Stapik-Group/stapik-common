#pragma once

#include "YearMonth.hpp"

#include <cstddef>
#include <iterator>

namespace stapik::domain
{
    // Inclusive range of months, iterable with a range-based for loop.
    class YearMonthRange
    {
    public:
        class Iterator
        {
        public:
            using iterator_concept = std::input_iterator_tag;
            using iterator_category = std::input_iterator_tag;
            using value_type = YearMonth;
            using difference_type = std::ptrdiff_t;

            Iterator() = default;

            [[nodiscard]] YearMonth operator*() const;

            Iterator& operator++();
            Iterator operator++(int);

            [[nodiscard]] bool operator==(const Iterator& other) const = default;
        private:
            friend class YearMonthRange;

            Iterator(const YearMonth& first, int offset);

            YearMonth m_first;
            int m_offset = 0;
        };

        // Throws std::invalid_argument when last precedes first.
        YearMonthRange(const YearMonth& first, const YearMonth& last);

        [[nodiscard]] const YearMonth& first() const;
        [[nodiscard]] const YearMonth& last() const;
        [[nodiscard]] int size() const;
        [[nodiscard]] bool contains(const YearMonth& month) const;

        [[nodiscard]] Iterator begin() const;
        [[nodiscard]] Iterator end() const;
    private:
        YearMonth m_first;
        YearMonth m_last;
    };
}
