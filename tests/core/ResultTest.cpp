#include "stapik/core/Result.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace
{
    using stapik::BadResultAccess;
    using stapik::Failure;
    using stapik::Result;

    struct Problem
    {
        int code = 0;
        std::string description;
    };

    Result<int, Problem> parsePositive(const int input)
    {
        if (input <= 0)
            return Failure(Problem{ .code = 22, .description = "not positive" });

        return input;
    }

    Result<void, Problem> requirePositive(const int input)
    {
        if (input <= 0)
            return Failure(Problem{ .code = 22, .description = "not positive" });

        return {};
    }
}

TEST(ResultTest, HoldsAValueOnSuccess)
{
    const auto result = parsePositive(5);

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 5);
    EXPECT_EQ(*result, 5);
}

TEST(ResultTest, HoldsTheErrorOnFailure)
{
    const auto result = parsePositive(-1);

    ASSERT_FALSE(result);
    EXPECT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().code, 22);
    EXPECT_EQ(result.error().description, "not positive");
}

TEST(ResultTest, ValueOfAFailureThrows)
{
    const auto result = parsePositive(0);

    EXPECT_THROW(static_cast<void>(result.value()), BadResultAccess);
}

TEST(ResultTest, ErrorOfASuccessThrows)
{
    const auto result = parsePositive(1);

    EXPECT_THROW(static_cast<void>(result.error()), BadResultAccess);
}

TEST(ResultTest, ValueOrFallsBackOnlyOnFailure)
{
    EXPECT_EQ(parsePositive(7).valueOr(-1), 7);
    EXPECT_EQ(parsePositive(0).valueOr(-1), -1);
}

TEST(ResultTest, ArrowReachesMembersOfTheValue)
{
    const Result<std::string, Problem> result{ std::string("hello") };

    EXPECT_EQ(result->size(), 5u);
}

TEST(ResultTest, ValueAndErrorMayBeTheSameType)
{
    const Result<std::string, std::string> succeeded{ std::string("value") };
    const Result<std::string, std::string> failed{ Failure(std::string("reason")) };

    ASSERT_TRUE(succeeded);
    EXPECT_EQ(succeeded.value(), "value");
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), "reason");
}

TEST(ResultTest, MovesAValueOutOfAnRvalueResult)
{
    Result<std::unique_ptr<int>, Problem> result{ std::make_unique<int>(42) };

    const auto moved = std::move(result).value();

    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(*moved, 42);
}

TEST(ResultTest, MovesAnErrorOutOfAnRvalueResult)
{
    auto result = parsePositive(-1);

    const auto problem = std::move(result).error();

    EXPECT_EQ(problem.description, "not positive");
}

TEST(ResultTest, VoidResultSucceedsByDefault)
{
    const auto result = requirePositive(3);

    EXPECT_TRUE(result);
    EXPECT_TRUE(result.hasValue());
}

TEST(ResultTest, VoidResultHoldsTheErrorOnFailure)
{
    const auto result = requirePositive(0);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, 22);
}

TEST(ResultTest, ErrorOfASuccessfulVoidResultThrows)
{
    const auto result = requirePositive(1);

    EXPECT_THROW(static_cast<void>(result.error()), BadResultAccess);
}
