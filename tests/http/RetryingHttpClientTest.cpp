#include "stapik/http/RetryingHttpClient.hpp"

#include "support/FakeHttpClient.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using namespace stapik::http;
    using stapik::test::FakeHttpClient;

    class RetryingHttpClientTest : public testing::Test
    {
    protected:
        [[nodiscard]] std::chrono::milliseconds totalSlept() const
        {
            std::chrono::milliseconds total{};
            for (const auto step : m_slept)
                total += step;
            return total;
        }

        FakeHttpClient m_inner;
        mutable std::vector<std::chrono::milliseconds> m_slept;
        RetryingHttpClient m_client{ m_inner, {}, [this](const std::chrono::milliseconds step) { m_slept.push_back(step); } };
    };
}

TEST_F(RetryingHttpClientTest, ResponseWithoutRetryAfterIsReturnedAtOnce)
{
    m_inner.respondWith(429);

    const auto response = m_client.send(Request{});

    ASSERT_TRUE(response);
    EXPECT_EQ(response.value().status, 429);
    EXPECT_EQ(m_inner.requests.size(), 1U);
}

TEST_F(RetryingHttpClientTest, ThrottledRequestIsSentAgainAfterTheRequestedWait)
{
    m_inner.respondWithRetryAfter(429, 2s);
    m_inner.respondWith(200, "ok");

    const auto response = m_client.send(Request{});

    ASSERT_TRUE(response);
    EXPECT_EQ(response.value().status, 200);
    EXPECT_EQ(m_inner.requests.size(), 2U);
    EXPECT_EQ(totalSlept(), 2000ms);
}

TEST_F(RetryingHttpClientTest, UnavailableServiceIsRetriedToo)
{
    m_inner.respondWithRetryAfter(503, 1s);
    m_inner.respondWith(200);

    EXPECT_EQ(m_client.send(Request{}).value().status, 200);
}

TEST_F(RetryingHttpClientTest, PostAndPatchAreNotRepeatedAfterUnavailable)
{
    for (const auto method : { Method::Post, Method::Patch })
    {
        m_inner.requests.clear();
        m_inner.respondWithRetryAfter(503, 1s);

        EXPECT_EQ(m_client.send(Request{ .method = method }).value().status, 503);
        EXPECT_EQ(m_inner.requests.size(), 1U);
    }
}

TEST_F(RetryingHttpClientTest, PostAndPatchAreRepeatedAfterThrottling)
{
    for (const auto method : { Method::Post, Method::Patch })
    {
        m_inner.requests.clear();
        m_inner.respondWithRetryAfter(429, 1s);
        m_inner.respondWith(200);

        EXPECT_EQ(m_client.send(Request{ .method = method }).value().status, 200);
        EXPECT_EQ(m_inner.requests.size(), 2U);
    }
}

TEST_F(RetryingHttpClientTest, OtherStatusesAreNeverRetried)
{
    m_inner.respondWithRetryAfter(500, 1s);

    EXPECT_EQ(m_client.send(Request{}).value().status, 500);
    EXPECT_EQ(m_inner.requests.size(), 1U);
}

TEST_F(RetryingHttpClientTest, GivesUpAfterTheConfiguredNumberOfRetries)
{
    for (int i = 0; i < 5; ++i)
        m_inner.respondWithRetryAfter(429, 1s);

    const auto response = m_client.send(Request{});

    EXPECT_EQ(response.value().status, 429);
    EXPECT_EQ(m_inner.requests.size(), 3U);
}

TEST_F(RetryingHttpClientTest, TooLongWaitIsNotWaitedFor)
{
    m_inner.respondWithRetryAfter(429, 31s);

    EXPECT_EQ(m_client.send(Request{}).value().status, 429);
    EXPECT_EQ(m_inner.requests.size(), 1U);
    EXPECT_TRUE(m_slept.empty());
}

TEST_F(RetryingHttpClientTest, TransportFailureIsReturnedWithoutRetrying)
{
    m_inner.failWith("Connection refused");

    const auto response = m_client.send(Request{});

    ASSERT_FALSE(response);
    EXPECT_EQ(response.error().message, "Connection refused");
    EXPECT_EQ(m_inner.requests.size(), 1U);
}

TEST_F(RetryingHttpClientTest, CancellingDuringTheWaitStopsTheRetry)
{
    m_inner.respondWithRetryAfter(429, 5s);
    m_inner.respondWith(200);
    bool cancelled = false;
    RetryingHttpClient client(m_inner, {}, [&](std::chrono::milliseconds) { cancelled = true; });

    const auto response = client.send(Request{ .isCancelled = [&] { return cancelled; } });

    ASSERT_FALSE(response);
    EXPECT_TRUE(response.error().cancelled);
    EXPECT_EQ(m_inner.requests.size(), 1U);
}
