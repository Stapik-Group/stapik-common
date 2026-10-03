#include "stapik/domain/Money.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace
{
    using stapik::domain::Currency;
    using stapik::domain::Money;

    Currency pln()
    {
        return Currency{ "PLN", "zł", false, 2 };
    }

    Currency eur()
    {
        return Currency{ "EUR", "€", false, 2 };
    }

    Currency yen()
    {
        return Currency{ "JPY", "¥", true, 0 };
    }
}

TEST(CurrencyTest, MinorUnitsPerMajorFollowsDecimalPlaces)
{
    EXPECT_EQ(pln().minorUnitsPerMajor(), 100);
    EXPECT_EQ(yen().minorUnitsPerMajor(), 1);
    EXPECT_EQ((Currency{ "KWD", "KD", false, 3 }.minorUnitsPerMajor()), 1000);
}

TEST(CurrencyTest, NameKeyPointsAtTheCurrencyTranslation)
{
    EXPECT_EQ(pln().nameKey(), "currency.PLN");
}

TEST(MoneyTest, StoresMinorUnitsAndCurrency)
{
    const Money money(12345, pln());

    EXPECT_EQ(money.minorUnits(), 12345);
    EXPECT_EQ(money.currency().code, "PLN");
    EXPECT_FALSE(money.isZero());
    EXPECT_FALSE(money.isNegative());
}

TEST(MoneyTest, ZeroAndNegative)
{
    EXPECT_TRUE(Money::zero(pln()).isZero());
    EXPECT_TRUE(Money(-1, pln()).isNegative());
}

TEST(MoneyTest, FromMajorRoundsToTheNearestMinorUnit)
{
    EXPECT_EQ(Money::fromMajor(19.99, pln()).minorUnits(), 1999);
    EXPECT_EQ(Money::fromMajor(0.1 + 0.2, pln()).minorUnits(), 30);
    EXPECT_EQ(Money::fromMajor(-5.5, pln()).minorUnits(), -550);
    EXPECT_EQ(Money::fromMajor(1234.0, yen()).minorUnits(), 1234);
}

TEST(MoneyTest, ToMajorUsesTheCurrencyScale)
{
    EXPECT_DOUBLE_EQ(Money(12345, pln()).toMajor(), 123.45);
    EXPECT_DOUBLE_EQ(Money(500, yen()).toMajor(), 500.0);
}

TEST(MoneyTest, AddsAndSubtractsWithinOneCurrency)
{
    const Money sum = Money(1000, pln()) + Money(250, pln());
    const Money difference = Money(1000, pln()) - Money(1500, pln());

    EXPECT_EQ(sum.minorUnits(), 1250);
    EXPECT_EQ(difference.minorUnits(), -500);
    EXPECT_EQ((-Money(300, pln())).minorUnits(), -300);
}

TEST(MoneyTest, CompoundAssignmentUpdatesTheValue)
{
    Money money(100, pln());

    money += Money(50, pln());
    money -= Money(30, pln());

    EXPECT_EQ(money.minorUnits(), 120);
}

TEST(MoneyTest, DifferentCurrenciesCannotBeCombinedOrOrdered)
{
    EXPECT_THROW(static_cast<void>(Money(1, pln()) + Money(1, eur())), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Money(1, pln()) - Money(1, eur())), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Money(1, pln()) < Money(2, eur())), std::invalid_argument);
}

TEST(MoneyTest, EqualityRequiresTheSameCurrencyAndAmount)
{
    EXPECT_TRUE(Money(100, pln()) == Money(100, pln()));
    EXPECT_FALSE(Money(100, pln()) == Money(101, pln()));
    EXPECT_FALSE(Money(100, pln()) == Money(100, eur()));
}

TEST(MoneyTest, OrdersByAmountWithinACurrency)
{
    EXPECT_TRUE(Money(100, pln()) < Money(200, pln()));
    EXPECT_TRUE(Money(300, pln()) > Money(200, pln()));
    EXPECT_TRUE(Money(200, pln()) <= Money(200, pln()));
}

TEST(MoneyTest, OverflowIsReportedInsteadOfWrapping)
{
    const auto maxValue = std::numeric_limits<std::int64_t>::max();
    const auto minValue = std::numeric_limits<std::int64_t>::min();

    EXPECT_THROW(static_cast<void>(Money(maxValue, pln()) + Money(1, pln())), std::overflow_error);
    EXPECT_THROW(static_cast<void>(Money(minValue, pln()) + Money(-1, pln())), std::overflow_error);
    EXPECT_THROW(static_cast<void>(-Money(minValue, pln())), std::overflow_error);
    EXPECT_EQ((Money(maxValue - 1, pln()) + Money(1, pln())).minorUnits(), maxValue);
}
