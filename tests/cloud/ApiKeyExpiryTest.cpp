#include "stapik/cloud/ApiKeyExpiry.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>

namespace
{
    using namespace std::chrono_literals;
    using stapik::cloud::ApiKeyExpiryMonitor;
    using stapik::cloud::parseApiKeyExpiry;
    using stapik::http::Response;

    const std::chrono::system_clock::time_point EXPIRY = std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::November / 1 } + 8h;

    Response responseWithExpiry(const std::string& value)
    {
        return Response{ .status = 200, .headers = { { "x-api-key-expires-at", value } } };
    }
}

TEST(ApiKeyExpiryTest, HeaderIsParsedAsIsoTimestamp)
{
    EXPECT_EQ(parseApiKeyExpiry(responseWithExpiry("2026-11-01T08:00:00Z")), std::optional{ EXPIRY });
}

TEST(ApiKeyExpiryTest, MissingHeaderMeansNoExpiry)
{
    EXPECT_FALSE(parseApiKeyExpiry(Response{ .status = 200 }).has_value());
}

TEST(ApiKeyExpiryTest, UnreadableHeaderMeansNoExpiry)
{
    EXPECT_FALSE(parseApiKeyExpiry(responseWithExpiry("soon")).has_value());
}

TEST(ApiKeyExpiryTest, MonitorKeepsTheLastReportedExpiry)
{
    ApiKeyExpiryMonitor monitor;
    EXPECT_FALSE(monitor.expiresAt().has_value());

    monitor.observe(responseWithExpiry("2026-11-01T08:00:00Z"));

    EXPECT_EQ(monitor.expiresAt(), std::optional{ EXPIRY });
}

TEST(ApiKeyExpiryTest, AnswerWithoutTheHeaderClearsTheExpiry)
{
    ApiKeyExpiryMonitor monitor;
    monitor.observe(responseWithExpiry("2026-11-01T08:00:00Z"));

    monitor.observe(Response{ .status = 200 });

    EXPECT_FALSE(monitor.expiresAt().has_value());
}

TEST(ApiKeyExpiryTest, ResetForgetsTheExpiry)
{
    ApiKeyExpiryMonitor monitor;
    monitor.observe(responseWithExpiry("2026-11-01T08:00:00Z"));

    monitor.reset();

    EXPECT_FALSE(monitor.expiresAt().has_value());
}
