#pragma once

#include "Currency.hpp"

#include <compare>
#include <cstdint>
#include <utility>

namespace stapik::domain
{
    class Money
    {
    public:
        Money() = default;
        Money(std::int64_t minorUnits, Currency currency);

        [[nodiscard]] static Money zero(Currency currency);
        [[nodiscard]] static Money fromMajor(double amount, Currency currency);

        [[nodiscard]] std::int64_t minorUnits() const;
        [[nodiscard]] const Currency& currency() const;
        [[nodiscard]] double toMajor() const;
        [[nodiscard]] bool isZero() const;
        [[nodiscard]] bool isNegative() const;

        [[nodiscard]] Money operator+(const Money& other) const;
        [[nodiscard]] Money operator-(const Money& other) const;
        [[nodiscard]] Money operator-() const;

        Money& operator+=(const Money& other);
        Money& operator-=(const Money& other);

        [[nodiscard]] bool operator==(const Money& other) const;
        [[nodiscard]] std::strong_ordering operator<=>(const Money& other) const;

    private:
        void requireSameCurrency(const Money& other) const;

        std::int64_t m_minorUnits = 0;
        Currency m_currency;
    };
}
