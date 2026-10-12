#include "stapik/domain/Money.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
    using stapik::domain::Currency;
    using stapik::domain::Money;
    using stapik::domain::RoundingMode;

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

    std::vector<std::int64_t> minorUnitsOf(const std::vector<Money>& parts)
    {
        std::vector<std::int64_t> result;
        for (const Money& part : parts)
            result.push_back(part.minorUnits());
        return result;
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

TEST(MoneyTest, MultipliesByAnInteger)
{
    Money money(250, pln());

    EXPECT_EQ((money * 3).minorUnits(), 750);
    EXPECT_EQ((money * -2).minorUnits(), -500);
    EXPECT_TRUE((money * 0).isZero());
    EXPECT_EQ((money * 3).currency().code, "PLN");

    money *= 4;
    EXPECT_EQ(money.minorUnits(), 1000);
}

TEST(MoneyTest, MultiplicationOverflowIsReported)
{
    const auto maxValue = std::numeric_limits<std::int64_t>::max();
    const auto minValue = std::numeric_limits<std::int64_t>::min();

    EXPECT_THROW(static_cast<void>(Money(maxValue, pln()) * 2), std::overflow_error);
    EXPECT_THROW(static_cast<void>(Money(minValue, pln()) * -1), std::overflow_error);
    EXPECT_EQ((Money(minValue, pln()) * 1).minorUnits(), minValue);
    EXPECT_EQ((Money(maxValue, pln()) * -1).minorUnits(), -maxValue);
}

TEST(MoneyTest, FromMajorRejectsAmountsOutsideTheInt64Range)
{
    EXPECT_THROW(static_cast<void>(Money::fromMajor(1e300, pln())), std::overflow_error);
    EXPECT_THROW(static_cast<void>(Money::fromMajor(-1e300, pln())), std::overflow_error);
    EXPECT_THROW(static_cast<void>(Money::fromMajor(std::numeric_limits<double>::infinity(), pln())), std::overflow_error);
    EXPECT_THROW(static_cast<void>(Money::fromMajor(std::numeric_limits<double>::quiet_NaN(), pln())), std::overflow_error);
}

TEST(MoneyTest, MultipliesByAFractionAndRounds)
{
    EXPECT_EQ(Money(1000, pln()).multiply(1, 3).minorUnits(), 333);
    EXPECT_EQ(Money(1000, pln()).multiply(2, 3).minorUnits(), 667);
    EXPECT_EQ(Money(7, pln()).multiply(1, 3).minorUnits(), 2);
    EXPECT_EQ(Money(10, pln()).multiply(1, -2).minorUnits(), -5);
    EXPECT_EQ(Money(10, pln()).multiply(3, 2).currency().code, "PLN");
}

TEST(MoneyTest, HalfUpRoundsTiesAwayFromZero)
{
    EXPECT_EQ(Money(5, pln()).multiply(1, 2, RoundingMode::HalfUp).minorUnits(), 3);
    EXPECT_EQ(Money(15, pln()).multiply(1, 2, RoundingMode::HalfUp).minorUnits(), 8);
    EXPECT_EQ(Money(-5, pln()).multiply(1, 2, RoundingMode::HalfUp).minorUnits(), -3);
    EXPECT_EQ(Money(5, pln()).multiply(1, 2).minorUnits(), 3);
}

TEST(MoneyTest, HalfEvenRoundsTiesToTheEvenNeighbour)
{
    EXPECT_EQ(Money(5, pln()).multiply(1, 2, RoundingMode::HalfEven).minorUnits(), 2);
    EXPECT_EQ(Money(15, pln()).multiply(1, 2, RoundingMode::HalfEven).minorUnits(), 8);
    EXPECT_EQ(Money(25, pln()).multiply(1, 10, RoundingMode::HalfEven).minorUnits(), 2);
    EXPECT_EQ(Money(-5, pln()).multiply(1, 2, RoundingMode::HalfEven).minorUnits(), -2);
    EXPECT_EQ(Money(-15, pln()).multiply(1, 2, RoundingMode::HalfEven).minorUnits(), -8);
}

TEST(MoneyTest, NonTiesRoundToTheNearestRegardlessOfMode)
{
    EXPECT_EQ(Money(1000, pln()).multiply(2, 3, RoundingMode::HalfEven).minorUnits(), 667);
    EXPECT_EQ(Money(-1000, pln()).multiply(2, 3, RoundingMode::HalfUp).minorUnits(), -667);
    EXPECT_EQ(Money(-1000, pln()).multiply(2, 3, RoundingMode::HalfEven).minorUnits(), -667);
}

TEST(MoneyTest, FractionNeedsANonZeroDenominator)
{
    EXPECT_THROW(static_cast<void>(Money(100, pln()).multiply(1, 0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Money(100, pln()).percent(10, 0)), std::invalid_argument);
}

TEST(MoneyTest, FractionUsesAWideIntermediateProduct)
{
    const auto maxValue = std::numeric_limits<std::int64_t>::max();

    EXPECT_EQ(Money(maxValue, pln()).multiply(2, 2).minorUnits(), maxValue);
    EXPECT_THROW(static_cast<void>(Money(maxValue, pln()).multiply(2, 1)), std::overflow_error);
    EXPECT_THROW(static_cast<void>(Money(maxValue, pln()).percent(101)), std::overflow_error);
}

TEST(MoneyTest, PercentOfAnAmount)
{
    EXPECT_EQ(Money(10000, pln()).percent(23).minorUnits(), 2300);
    EXPECT_EQ(Money(1999, pln()).percent(23).minorUnits(), 460);
    EXPECT_EQ(Money(1050, pln()).percent(50).minorUnits(), 525);
    EXPECT_EQ(Money(10000, pln()).percent(235, 10).minorUnits(), 2350);
    EXPECT_EQ(Money(10000, pln()).percent(-10).minorUnits(), -1000);
}

TEST(MoneyTest, PercentHonoursTheRoundingMode)
{
    EXPECT_EQ(Money(5, pln()).percent(50).minorUnits(), 3);
    EXPECT_EQ(Money(5, pln()).percent(50, 1, RoundingMode::HalfEven).minorUnits(), 2);
    EXPECT_EQ(Money(1, pln()).percent(150, 1, RoundingMode::HalfEven).minorUnits(), 2);
}

TEST(MoneyTest, AllocateSplitsEvenlyAndGivesTheLeftoverToTheFirstParts)
{
    EXPECT_EQ(minorUnitsOf(Money(100, pln()).allocate(3)), (std::vector<std::int64_t>{ 34, 33, 33 }));
    EXPECT_EQ(minorUnitsOf(Money(1000, pln()).allocate(4)), (std::vector<std::int64_t>{ 250, 250, 250, 250 }));
    EXPECT_EQ(minorUnitsOf(Money(2, pln()).allocate(5)), (std::vector<std::int64_t>{ 1, 1, 0, 0, 0 }));
    EXPECT_EQ(minorUnitsOf(Money::zero(pln()).allocate(3)), (std::vector<std::int64_t>{ 0, 0, 0 }));
}

TEST(MoneyTest, AllocateHandlesNegativeAmounts)
{
    EXPECT_EQ(minorUnitsOf(Money(-100, pln()).allocate(3)), (std::vector<std::int64_t>{ -34, -33, -33 }));
    EXPECT_EQ(minorUnitsOf(Money(-2, pln()).allocate(3)), (std::vector<std::int64_t>{ -1, -1, 0 }));
}

TEST(MoneyTest, AllocateKeepsTheCurrencyAndNeedsAtLeastOnePart)
{
    EXPECT_EQ(Money(100, eur()).allocate(2).front().currency().code, "EUR");
    EXPECT_EQ(minorUnitsOf(Money(100, pln()).allocate(1)), (std::vector<std::int64_t>{ 100 }));
    EXPECT_THROW(static_cast<void>(Money(100, pln()).allocate(0)), std::invalid_argument);
}

TEST(MoneyTest, AllocateProportionallyFollowsTheRatios)
{
    EXPECT_EQ(minorUnitsOf(Money(100, pln()).allocateProportionally({ 70, 20, 10 })), (std::vector<std::int64_t>{ 70, 20, 10 }));
    EXPECT_EQ(minorUnitsOf(Money(100, pln()).allocateProportionally({ 1, 1, 1 })), (std::vector<std::int64_t>{ 34, 33, 33 }));
    EXPECT_EQ(minorUnitsOf(Money(-100, pln()).allocateProportionally({ 1, 1, 1 })), (std::vector<std::int64_t>{ -34, -33, -33 }));
}

TEST(MoneyTest, AllocateProportionallyGivesLeftoverToTheLargestFractionalShare)
{
    EXPECT_EQ(minorUnitsOf(Money(1000, pln()).allocateProportionally({ 1, 2 })), (std::vector<std::int64_t>{ 333, 667 }));
    EXPECT_EQ(minorUnitsOf(Money(5, pln()).allocateProportionally({ 1, 1 })), (std::vector<std::int64_t>{ 3, 2 }));
}

TEST(MoneyTest, AllocateProportionallyNeverGivesMoneyToAZeroRatio)
{
    EXPECT_EQ(minorUnitsOf(Money(10, pln()).allocateProportionally({ 0, 1, 1 })), (std::vector<std::int64_t>{ 0, 5, 5 }));
    EXPECT_EQ(minorUnitsOf(Money(1, pln()).allocateProportionally({ 0, 1, 1 })), (std::vector<std::int64_t>{ 0, 1, 0 }));
}

TEST(MoneyTest, AllocateProportionallyRejectsInvalidRatios)
{
    EXPECT_THROW(static_cast<void>(Money(100, pln()).allocateProportionally({})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Money(100, pln()).allocateProportionally({ 1, -1 })), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Money(100, pln()).allocateProportionally({ 0, 0 })), std::invalid_argument);
}

TEST(MoneyTest, AllocateProportionallyDoesNotOverflowOnExtremeValues)
{
    const auto maxValue = std::numeric_limits<std::int64_t>::max();
    const auto minValue = std::numeric_limits<std::int64_t>::min();

    EXPECT_EQ(
        minorUnitsOf(Money(maxValue, pln()).allocateProportionally({ maxValue, maxValue })),
        (std::vector<std::int64_t>{ maxValue / 2 + 1, maxValue / 2 }));
    EXPECT_EQ(
        minorUnitsOf(Money(minValue, pln()).allocateProportionally({ maxValue, maxValue })),
        (std::vector<std::int64_t>{ minValue / 2, minValue / 2 }));
}
