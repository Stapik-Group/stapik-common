#include "stapik/http/HttpMessage.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace stapik::http;
}

TEST(HttpMessageTest, TextSurvivesRoundTripThroughBytes)
{
    const std::string text = "zażółć gęślą jaźń\n";

    EXPECT_EQ(toText(toBytes(text)), text);
}

TEST(HttpMessageTest, BytesKeepEmbeddedNulCharacters)
{
    const std::string text("a\0b", 3);

    const auto bytes = toBytes(text);

    EXPECT_EQ(bytes.size(), 3U);
    EXPECT_EQ(toText(bytes), text);
}

TEST(HttpMessageTest, EmptyTextGivesEmptyBytes)
{
    EXPECT_TRUE(toBytes("").empty());
    EXPECT_TRUE(toText({}).empty());
}

TEST(HttpMessageTest, RequestHasConnectAndStallTimeoutsByDefault)
{
    const Request request;

    EXPECT_EQ(request.connectTimeout, std::chrono::seconds(10));
    EXPECT_EQ(request.stallTimeout, std::chrono::seconds(30));
    EXPECT_NE(request.timeout, NO_TIMEOUT);
}

TEST(HttpMessageTest, RequestDefaultsToBodylessGet)
{
    const Request request;

    EXPECT_EQ(request.method, Method::Get);
    EXPECT_TRUE(std::holds_alternative<NoBody>(request.body));
    EXPECT_FALSE(request.maxResponseBytes.has_value());
}
