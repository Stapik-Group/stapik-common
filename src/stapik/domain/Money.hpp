#pragma once

#include "Currency.hpp"

#include <compare>
#include <cstddef>
#include <vector>

namespace stapik::domain
{
    enum class RoundingMode
    {
        HalfUp,
        HalfEven,
    };

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

        [[nodiscard]] Money operator*(std::int64_t factor) const;
        Money& operator*=(std::int64_t factor);

        [[nodiscard]] Money multiply(
            std::int64_t numerator,
            std::int64_t denominator,
            RoundingMode mode = RoundingMode::HalfUp) const;

        [[nodiscard]] Money percent(
            std::int64_t numerator,
            std::int64_t denominator = 1,
            RoundingMode mode = RoundingMode::HalfUp) const;

        [[nodiscard]] std::vector<Money> allocate(std::size_t parts) const;

        [[nodiscard]] std::vector<Money> allocateProportionally(const std::vector<std::int64_t>& ratios) const;

        [[nodiscard]] bool operator==(const Money& other) const;
        [[nodiscard]] std::strong_ordering operator<=>(const Money& other) const;

    private:
        void requireSameCurrency(const Money& other) const;

        std::int64_t m_minorUnits = 0;
        Currency m_currency;
    };
}
