#include "stapik/http/CurlHttpClient.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace stapik::http;
using namespace std::chrono_literals;

namespace
{
    Request unreachableRequest(RequestBody body = NoBody{})
    {
        return Request{
            .method = Method::Post,
            .url = "http://127.0.0.1:1/unreachable",
            .body = std::move(body),
            .timeout = 5s,
            .connectTimeout = 2s
        };
    }

    MultipartFile smallFile()
    {
        return MultipartFile{ .fieldName = "file", .filename = "a.bin", .mimeType = "application/octet-stream", .content = Bytes{ 1, 2, 3 } };
    }
}

TEST(CurlHttpClientTest, FailedRequestsOnTheSameThreadKeepFailingCleanly)
{
    const CurlHttpClient client;

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto result = client.send(unreachableRequest());
        ASSERT_FALSE(result.hasValue());
        EXPECT_FALSE(result.error().cancelled);
    }
}

TEST(CurlHttpClientTest, HandleIsUsableAfterAMultipartRequest)
{
    const CurlHttpClient client;

    EXPECT_FALSE(client.send(unreachableRequest(smallFile())).hasValue());
    EXPECT_FALSE(client.send(unreachableRequest()).hasValue());
    EXPECT_FALSE(client.send(unreachableRequest(smallFile())).hasValue());
}

TEST(CurlHttpClientTest, RequestsFromSeveralThreadsFailIndependently)
{
    const CurlHttpClient client;
    std::atomic<int> failures{ 0 };

    {
        std::vector<std::jthread> threads;
        for (int index = 0; index < 4; ++index)
        {
            threads.emplace_back([&]
            {
                for (int attempt = 0; attempt < 2; ++attempt)
                    if (!client.send(unreachableRequest()).hasValue())
                        ++failures;
            });
        }
    }

    EXPECT_EQ(failures.load(), 8);
}

TEST(CurlHttpClientTest, CancelledRequestDoesNotSpoilTheNextOne)
{
    const CurlHttpClient client;

    auto cancelled = unreachableRequest();
    cancelled.isCancelled = [] { return true; };
    client.send(cancelled);

    const auto result = client.send(unreachableRequest());
    ASSERT_FALSE(result.hasValue());
    EXPECT_FALSE(result.error().cancelled);
}
