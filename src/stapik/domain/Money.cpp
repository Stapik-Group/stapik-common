#include "Money.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace stapik::domain
{
    Money::Money(const std::int64_t minorUnits, Currency currency) :
        m_minorUnits(minorUnits),
        m_currency(std::move(currency))
    {}

    Money Money::zero(Currency currency)
    {
        return {0, std::move(currency)};
    }

    Money Money::fromMajor(const double amount, Currency currency)
    {
        const auto scaled = std::llround(amount * static_cast<double>(currency.minorUnitsPerMajor()));
        return {scaled, std::move(currency)};
    }

    std::int64_t Money::minorUnits() const
    {
        return m_minorUnits;
    }

    const Currency& Money::currency() const
    {
        return m_currency;
    }

    double Money::toMajor() const
    {
        return static_cast<double>(m_minorUnits) / static_cast<double>(m_currency.minorUnitsPerMajor());
    }

    bool Money::isZero() const
    {
        return m_minorUnits == 0;
    }

    bool Money::isNegative() const
    {
        return m_minorUnits < 0;
    }

    Money Money::operator+(const Money& other) const
    {
        requireSameCurrency(other);

        if ((other.m_minorUnits > 0 && m_minorUnits > std::numeric_limits<std::int64_t>::max() - other.m_minorUnits)
            || (other.m_minorUnits < 0 && m_minorUnits < std::numeric_limits<std::int64_t>::min() - other.m_minorUnits))
        {
            throw std::overflow_error("Money addition overflows");
        }

        return {m_minorUnits + other.m_minorUnits, m_currency};
    }

    Money Money::operator-(const Money& other) const
    {
        return *this + -other;
    }

    Money Money::operator-() const
    {
        if (m_minorUnits == std::numeric_limits<std::int64_t>::min())
            throw std::overflow_error("Money negation overflows");

        return {-m_minorUnits, m_currency};
    }

    Money& Money::operator+=(const Money& other)
    {
        *this = *this + other;
        return *this;
    }

    Money& Money::operator-=(const Money& other)
    {
        *this = *this - other;
        return *this;
    }

    bool Money::operator==(const Money& other) const
    {
        return m_minorUnits == other.m_minorUnits && m_currency.code == other.m_currency.code;
    }

    std::strong_ordering Money::operator<=>(const Money& other) const
    {
        requireSameCurrency(other);
        return m_minorUnits <=> other.m_minorUnits;
    }

    void Money::requireSameCurrency(const Money& other) const
    {
        if (m_currency.code != other.m_currency.code)
            throw std::invalid_argument("Money values have different currencies");
    }
}
