#include "Money.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace
{
    __extension__ typedef __int128 Int128;
    __extension__ typedef unsigned __int128 UInt128;

    constexpr double TWO_POW_63 = 9223372036854775808.0;

    std::int64_t toInt64(const Int128 value, const char* overflowMessage)
    {
        if (value > std::numeric_limits<std::int64_t>::max() || value < std::numeric_limits<std::int64_t>::min())
            throw std::overflow_error(overflowMessage);

        return static_cast<std::int64_t>(value);
    }

    Int128 roundedQuotient(Int128 numerator, Int128 denominator, const stapik::domain::RoundingMode mode)
    {
        if (denominator == 0)
            throw std::invalid_argument("Money scaling needs a non-zero denominator");

        if (denominator < 0)
        {
            numerator = -numerator;
            denominator = -denominator;
        }

        Int128 quotient = numerator / denominator;
        const Int128 remainder = numerator % denominator;
        const Int128 twiceRemainder = (remainder < 0 ? -remainder : remainder) * 2;

        const bool isTie = twiceRemainder == denominator;

        if (const bool tieGoesAway = mode == stapik::domain::RoundingMode::HalfUp || quotient % 2 != 0; twiceRemainder > denominator || (isTie && tieGoesAway))
            quotient += numerator < 0 ? -1 : 1;

        return quotient;
    }
}

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
        const double scaled = std::round(amount * static_cast<double>(currency.minorUnitsPerMajor()));

        // Negated range check so that NaN is rejected as well.
        if (!(scaled >= -TWO_POW_63 && scaled < TWO_POW_63))
            throw std::overflow_error("Money amount is out of range");

        return {static_cast<std::int64_t>(scaled), std::move(currency)};
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

    Money Money::operator*(const std::int64_t factor) const
    {
        return {toInt64(static_cast<Int128>(m_minorUnits) * factor, "Money multiplication overflows"), m_currency};
    }

    Money& Money::operator*=(const std::int64_t factor)
    {
        *this = *this * factor;
        return *this;
    }

    Money Money::multiply(const std::int64_t numerator, const std::int64_t denominator, const RoundingMode mode) const
    {
        const Int128 scaled = roundedQuotient(static_cast<Int128>(m_minorUnits) * numerator, denominator, mode);
        return {toInt64(scaled, "Money multiplication overflows"), m_currency};
    }

    Money Money::percent(const std::int64_t numerator, const std::int64_t denominator, const RoundingMode mode) const
    {
        const Int128 scaled = roundedQuotient(
            static_cast<Int128>(m_minorUnits) * numerator,
            static_cast<Int128>(denominator) * 100,
            mode);
        return {toInt64(scaled, "Money multiplication overflows"), m_currency};
    }

    std::vector<Money> Money::allocate(const std::size_t parts) const
    {
        return allocateProportionally(std::vector<std::int64_t>(parts, 1));
    }

    std::vector<Money> Money::allocateProportionally(const std::vector<std::int64_t>& ratios) const
    {
        if (ratios.empty())
            throw std::invalid_argument("Money allocation needs at least one part");

        UInt128 totalRatio = 0;
        for (const std::int64_t ratio : ratios)
        {
            if (ratio < 0)
                throw std::invalid_argument("Money allocation ratios cannot be negative");

            totalRatio += static_cast<UInt128>(ratio);
        }

        if (totalRatio == 0)
            throw std::invalid_argument("Money allocation needs a positive ratio");

        const bool negative = m_minorUnits < 0;
        const auto amount = static_cast<UInt128>(negative ? -static_cast<Int128>(m_minorUnits) : m_minorUnits);

        std::vector<UInt128> shares(ratios.size());
        std::vector<UInt128> remainders(ratios.size());
        UInt128 distributed = 0;
        for (std::size_t i = 0; i < ratios.size(); ++i)
        {
            const UInt128 scaled = amount * static_cast<UInt128>(ratios[i]);
            shares[i] = scaled / totalRatio;
            remainders[i] = scaled % totalRatio;
            distributed += shares[i];
        }

        std::vector<std::size_t> byRemainder(ratios.size());
        std::iota(byRemainder.begin(), byRemainder.end(), std::size_t{0});
        std::ranges::stable_sort(
            byRemainder,
            [&remainders](const std::size_t left, const std::size_t right) { return remainders[left] > remainders[right]; });

        const auto leftover = static_cast<std::size_t>(amount - distributed);
        for (std::size_t i = 0; i < leftover; ++i)
            ++shares[byRemainder[i]];

        std::vector<Money> result;
        result.reserve(shares.size());
        for (const UInt128 share : shares)
        {
            const auto signedShare = negative ? -static_cast<Int128>(share) : static_cast<Int128>(share);
            result.emplace_back(static_cast<std::int64_t>(signedShare), m_currency);
        }
        return result;
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
