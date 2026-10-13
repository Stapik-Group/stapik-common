#include "stapik/cloud/RateLimit.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace
{
    using namespace std::chrono_literals;
    using stapik::cloud::parseRateLimit;
    using stapik::cloud::RateLimitState;
    using stapik::http::Response;

    Response withHeaders(std::map<std::string, std::string> headers)
    {
        return Response{ .status = 200, .headers = std::move(headers) };
    }
}

TEST(RateLimitTest, AllThreeHeadersGiveTheState)
{
    const auto state = parseRateLimit(withHeaders({ { "x-ratelimit-limit", "600" }, { "x-ratelimit-remaining", "598" }, { "x-ratelimit-reset", "42" } }));

    EXPECT_EQ(state, std::optional(RateLimitState{ .limit = 600, .remaining = 598, .resetAfter = 42s }));
}

TEST(RateLimitTest, ExhaustedAllowanceIsAValidState)
{
    const auto state = parseRateLimit(withHeaders({ { "x-ratelimit-limit", "600" }, { "x-ratelimit-remaining", "0" }, { "x-ratelimit-reset", "17" } }));

    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->remaining, 0);
}

TEST(RateLimitTest, MissingHeaderMeansNoState)
{
    EXPECT_FALSE(parseRateLimit(withHeaders({ { "x-ratelimit-limit", "600" }, { "x-ratelimit-remaining", "598" } })).has_value());
    EXPECT_FALSE(parseRateLimit(Response{ .status = 200 }).has_value());
}

TEST(RateLimitTest, UnreadableOrNegativeValueMeansNoState)
{
    EXPECT_FALSE(parseRateLimit(withHeaders({ { "x-ratelimit-limit", "many" }, { "x-ratelimit-remaining", "1" }, { "x-ratelimit-reset", "1" } })).has_value());
    EXPECT_FALSE(parseRateLimit(withHeaders({ { "x-ratelimit-limit", "600" }, { "x-ratelimit-remaining", "-1" }, { "x-ratelimit-reset", "1" } })).has_value());
    EXPECT_FALSE(parseRateLimit(withHeaders({ { "x-ratelimit-limit", "600" }, { "x-ratelimit-remaining", "5 left" }, { "x-ratelimit-reset", "1" } })).has_value());
}
