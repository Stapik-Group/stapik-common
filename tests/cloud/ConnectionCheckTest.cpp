#include "stapik/cloud/ConnectionCheck.hpp"

#include "support/FakeHttpClient.hpp"

#include <gtest/gtest.h>

#include <tuple>

namespace
{
    using namespace stapik::cloud;
    using stapik::http::Method;
    using stapik::test::FakeHttpClient;

    const CloudStorageConfig CONFIG{ .apiUrl = "https://cloud.example/", .apiKey = "secret-key" };

    class ConnectionCheckTest : public testing::Test
    {
    protected:
        FakeHttpClient m_http;
    };
}

TEST_F(ConnectionCheckTest, SendsAuthenticatedGetToTheNormalizedAddress)
{
    m_http.respondWith(200, R"({"assets":[]})");

    std::ignore = checkConnection(CONFIG, m_http);

    ASSERT_EQ(m_http.requests.size(), 1U);
    const auto& request = m_http.requests.front();
    EXPECT_EQ(request.method, Method::Get);
    EXPECT_EQ(request.url, "https://cloud.example/api/v1/assets/connection-check");
    EXPECT_EQ(request.headers.at("x-api-key"), "secret-key");
}

TEST_F(ConnectionCheckTest, SucceedsWhenTheServerKnowsTheKey)
{
    m_http.respondWith(200, R"({"assets":[]})");
    EXPECT_TRUE(checkConnection(CONFIG, m_http));
}

TEST_F(ConnectionCheckTest, MissingProbeSlotStillMeansTheKeyWasAccepted)
{
    m_http.respondWith(404);
    EXPECT_TRUE(checkConnection(CONFIG, m_http));
}

TEST_F(ConnectionCheckTest, RejectedKeyIsReported)
{
    for (const int status : { 401, 403 })
    {
        m_http.respondWith(status);

        const auto result = checkConnection(CONFIG, m_http);

        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().kind, ConnectionErrorKind::InvalidApiKey);
        EXPECT_EQ(result.error().httpStatus, status);
    }
}

TEST_F(ConnectionCheckTest, OtherStatusIsReportedWithItsCode)
{
    m_http.respondWith(500);

    const auto result = checkConnection(CONFIG, m_http);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ConnectionErrorKind::UnexpectedStatus);
    EXPECT_EQ(result.error().httpStatus, 500);
}

TEST_F(ConnectionCheckTest, TransportFailureMeansTheServerIsUnreachable)
{
    m_http.failWith("could not resolve host");

    const auto result = checkConnection(CONFIG, m_http);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ConnectionErrorKind::Unreachable);
    EXPECT_EQ(result.error().detail, "could not resolve host");
}
