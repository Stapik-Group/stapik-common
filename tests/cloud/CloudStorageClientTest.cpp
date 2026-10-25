#include "stapik/cloud/CloudStorageClient.hpp"

#include "support/FakeHttpClient.hpp"
#include "support/ThrownMessage.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <typeinfo>

namespace
{
    using namespace std::chrono_literals;
    using stapik::http::Method;
    using stapik::test::bodyText;
    using stapik::test::FakeHttpClient;
    using stapik::test::thrownMessage;

    const CloudStorageConfig CONFIG{ .apiUrl = "https://cloud.example", .apiKey = "secret-key" };
    const std::string DOCUMENT_URL = "https://cloud.example/api/v1/documents/budget";
    const std::string PARTITIONS_URL = DOCUMENT_URL + "/partitions";
    const std::string SERVER_TIME_TEXT = "2026-01-15T10:30:00Z";
    const std::chrono::system_clock::time_point SERVER_TIME = std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::January / 15 } + 10h + 30min;

    std::string documentResponse(const nlohmann::json& content)
    {
        return nlohmann::json{ { "content", content.dump() }, { "updatedAt", SERVER_TIME_TEXT } }.dump();
    }

    class CloudStorageClientTest : public testing::Test
    {
    protected:
        FakeHttpClient m_http;
        CloudStorageClient m_client{ CONFIG, "budget", m_http };

        // The answer to the read that precedes every save of an existing document.
        void respondWithCurrentDocument(const std::string& etag = "\"current\"") const
        {
            m_http.respondWithHeaders(200, documentResponse(nlohmann::json::object()), { { "etag", etag } });
        }
    };
}

TEST_F(CloudStorageClientTest, UnconfiguredClientNeverSendsARequest)
{
    const CloudStorageClient unconfigured(CloudStorageConfig{ .apiUrl = "https://cloud.example", .apiKey = {} }, "budget", m_http);

    EXPECT_FALSE(unconfigured.loadDocument().has_value());
    EXPECT_TRUE(unconfigured.listPartitions().empty());
    EXPECT_FALSE(unconfigured.loadPartition("2025").has_value());
    EXPECT_FALSE(unconfigured.deletePartition("2025"));
    EXPECT_THROW(std::ignore = unconfigured.saveDocument(nlohmann::json::object(), {}), CloudStorageException);
    EXPECT_THROW(std::ignore = unconfigured.savePartition("2025", nlohmann::json::object(), {}), CloudStorageException);
    EXPECT_TRUE(m_http.requests.empty());
}

TEST_F(CloudStorageClientTest, LoadSendsAuthenticatedGetToTheSlotUrl)
{
    m_http.respondWith(200, documentResponse(nlohmann::json::object()));

    std::ignore = m_client.loadDocument();

    ASSERT_EQ(m_http.requests.size(), 1U);
    const auto& request = m_http.requests.front();
    EXPECT_EQ(request.method, Method::Get);
    EXPECT_EQ(request.url, DOCUMENT_URL);
    EXPECT_EQ(request.headers.at("x-api-key"), "secret-key");
    EXPECT_EQ(request.headers.at("Content-Type"), "application/json");
    EXPECT_TRUE(std::holds_alternative<stapik::http::NoBody>(request.body));
}

TEST_F(CloudStorageClientTest, ApiUrlIsNormalizedBeforeBuildingTheUrl)
{
    const CloudStorageClient client(CloudStorageConfig{ .apiUrl = "  https://cloud.example/ ", .apiKey = "secret-key" }, "budget", m_http);
    m_http.respondWith(404);

    std::ignore = client.loadDocument();

    EXPECT_EQ(m_http.requests.front().url, DOCUMENT_URL);
}

TEST_F(CloudStorageClientTest, LoadParsesContentAndServerTime)
{
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    const auto document = m_client.loadDocument();

    ASSERT_TRUE(document.has_value());
    EXPECT_EQ(document->content, (nlohmann::json{ { "a", 1 } }));
    EXPECT_EQ(document->updatedAt, SERVER_TIME);
}

TEST_F(CloudStorageClientTest, LoadReadsTheEtagHeader)
{
    m_http.respondWithHeaders(200, documentResponse(nlohmann::json::object()), { { "etag", "\"hash-1\"" } });

    const auto document = m_client.loadDocument();

    ASSERT_TRUE(document.has_value());
    EXPECT_EQ(document->etag, "\"hash-1\"");
}

TEST_F(CloudStorageClientTest, LoadWithoutEtagHeaderGivesEmptyEtag)
{
    m_http.respondWith(200, documentResponse(nlohmann::json::object()));

    const auto document = m_client.loadDocument();

    ASSERT_TRUE(document.has_value());
    EXPECT_TRUE(document->etag.empty());
}

TEST_F(CloudStorageClientTest, EmptyContentLoadsAsEmptyObject)
{
    m_http.respondWith(200, nlohmann::json{ { "content", "" }, { "updatedAt", SERVER_TIME_TEXT } }.dump());

    const auto document = m_client.loadDocument();

    ASSERT_TRUE(document.has_value());
    EXPECT_EQ(document->content, nlohmann::json::object());
}

TEST_F(CloudStorageClientTest, MissingDocumentLoadsAsNullopt)
{
    m_http.respondWith(404);

    EXPECT_FALSE(m_client.loadDocument().has_value());
}

TEST_F(CloudStorageClientTest, UnexpectedStatusOnLoadIsReported)
{
    m_http.respondWith(500);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.loadDocument(); }), "API read failed with status 500");
}

TEST_F(CloudStorageClientTest, RejectedApiKeyOnLoadIsReportedAsAuthRejection)
{
    for (const int status : { 401, 403 })
    {
        m_http.respondWith(status);

        EXPECT_THROW(std::ignore = m_client.loadDocument(), CloudAuthRejectedException) << status;
    }
}

TEST_F(CloudStorageClientTest, RejectedApiKeyOnSaveIsReportedAsAuthRejection)
{
    respondWithCurrentDocument();
    m_http.respondWith(403);

    EXPECT_THROW(std::ignore = m_client.saveDocument(nlohmann::json::object(), SERVER_TIME), CloudAuthRejectedException);
}

TEST_F(CloudStorageClientTest, TooLargeDocumentOnSaveIsReportedAsSuch)
{
    respondWithCurrentDocument();
    m_http.respondWith(413);

    EXPECT_EQ(thrownMessage<CloudDocumentTooLargeException>([&] { std::ignore = m_client.saveDocument(nlohmann::json::object(), SERVER_TIME); }), "API write failed with status 413");
}

TEST_F(CloudStorageClientTest, RejectedApiKeyOnPartitionListAndChangeListIsReportedAsAuthRejection)
{
    m_http.respondWith(401);
    EXPECT_THROW(std::ignore = m_client.listPartitions(), CloudAuthRejectedException);

    m_http.respondWith(403);
    EXPECT_THROW(std::ignore = m_client.listChanges(SERVER_TIME), CloudAuthRejectedException);
}

TEST_F(CloudStorageClientTest, ServerErrorStaysAPlainCloudFailure)
{
    m_http.respondWith(503);

    try
    {
        std::ignore = m_client.loadDocument();
        FAIL() << "expected an exception";
    }
    catch (const CloudStorageException& exception)
    {
        EXPECT_EQ(typeid(exception), typeid(CloudStorageException));
    }
}

TEST_F(CloudStorageClientTest, TransportFailureOnLoadIsReportedWithTheReason)
{
    m_http.failWith("Timeout was reached");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.loadDocument(); }), "API read failed: Timeout was reached");
}

TEST_F(CloudStorageClientTest, MalformedResponseOnLoadIsReportedAsCloudError)
{
    m_http.respondWith(200, "not json");

    EXPECT_THROW(std::ignore = m_client.loadDocument(), CloudStorageException);
}

TEST_F(CloudStorageClientTest, ResponseWithInvalidTimestampIsReportedAsCloudError)
{
    m_http.respondWith(200, nlohmann::json{ { "content", "{}" }, { "updatedAt", "yesterday" } }.dump());

    EXPECT_THROW(std::ignore = m_client.loadDocument(), CloudStorageException);
}

TEST_F(CloudStorageClientTest, SendsContentWithIfMatchInAPut)
{
    respondWithCurrentDocument("\"hash-1\"");
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    std::ignore = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 2U);
    EXPECT_EQ(m_http.requests.front().method, Method::Get);
    EXPECT_EQ(m_http.requests.front().url, DOCUMENT_URL);

    const auto& request = m_http.requests.back();
    EXPECT_EQ(request.method, Method::Put);
    EXPECT_EQ(request.url, DOCUMENT_URL);
    EXPECT_EQ(request.headers.at("x-api-key"), "secret-key");
    EXPECT_EQ(request.headers.at("If-Match"), "\"hash-1\"");

    const auto payload = nlohmann::json::parse(bodyText(request));
    EXPECT_EQ(nlohmann::json::parse(payload.at("content").get<std::string>()), (nlohmann::json{ { "a", 1 } }));
    EXPECT_FALSE(payload.contains("clientLastKnownUpdate"));
}

TEST_F(CloudStorageClientTest, MissingDocumentIsCreatedWithoutIfMatch)
{
    m_http.respondWith(404);
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, {});

    EXPECT_FALSE(conflict);
    ASSERT_EQ(m_http.requests.size(), 2U);
    EXPECT_EQ(m_http.requests.back().method, Method::Put);
    EXPECT_FALSE(m_http.requests.back().headers.contains("If-Match"));
}

TEST_F(CloudStorageClientTest, StaleBaselineIsAConflictAndNothingIsSent)
{
    respondWithCurrentDocument("\"hash-4\"");

    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "local", true } }, SERVER_TIME - 1h);

    EXPECT_TRUE(conflict);
    EXPECT_EQ(document.content, nlohmann::json::object());
    EXPECT_EQ(document.updatedAt, SERVER_TIME);
    EXPECT_EQ(document.etag, "\"hash-4\"");
    ASSERT_EQ(m_http.requests.size(), 1U);
    EXPECT_EQ(m_http.requests.front().method, Method::Get);
}

TEST_F(CloudStorageClientTest, CurrentDocumentWithoutEtagIsWrittenWithoutIfMatch)
{
    m_http.respondWith(200, documentResponse(nlohmann::json::object()));
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    std::ignore = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 2U);
    EXPECT_FALSE(m_http.requests.back().headers.contains("If-Match"));
}

TEST_F(CloudStorageClientTest, UnexpectedStatusOnTheReadBeforeSaveIsReported)
{
    m_http.respondWith(500);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.saveDocument(nlohmann::json::object(), SERVER_TIME); }), "API read failed with status 500");
    EXPECT_EQ(m_http.requests.size(), 1U);
}

TEST_F(CloudStorageClientTest, AcceptedSaveReturnsTheStoredDocument)
{
    respondWithCurrentDocument();
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);

    EXPECT_FALSE(conflict);
    EXPECT_EQ(document.content, (nlohmann::json{ { "a", 1 } }));
    EXPECT_EQ(document.updatedAt, SERVER_TIME);
}

TEST_F(CloudStorageClientTest, AcceptedSaveReturnsTheNewEtag)
{
    respondWithCurrentDocument();
    m_http.respondWithHeaders(200, documentResponse(nlohmann::json{ { "a", 1 } }), { { "etag", "\"hash-2\"" } });

    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);

    EXPECT_EQ(document.etag, "\"hash-2\"");
}

TEST_F(CloudStorageClientTest, ConflictingSaveReturnsTheServerEtag)
{
    respondWithCurrentDocument();
    m_http.respondWithHeaders(409, documentResponse(nlohmann::json{ { "server", true } }), { { "etag", "\"hash-3\"" } });

    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "local", true } }, SERVER_TIME);

    EXPECT_TRUE(conflict);
    EXPECT_EQ(document.etag, "\"hash-3\"");
}

TEST_F(CloudStorageClientTest, ConflictingSaveReturnsTheServerVersion)
{
    respondWithCurrentDocument();
    m_http.respondWith(409, documentResponse(nlohmann::json{ { "server", true } }));

    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "local", true } }, SERVER_TIME);

    EXPECT_TRUE(conflict);
    EXPECT_EQ(document.content, (nlohmann::json{ { "server", true } }));
}

TEST_F(CloudStorageClientTest, UnexpectedStatusOnSaveIsReported)
{
    respondWithCurrentDocument();
    m_http.respondWith(401);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.saveDocument(nlohmann::json::object(), SERVER_TIME); }), "API write failed with status 401");
}

TEST_F(CloudStorageClientTest, TransportFailureOnSaveIsReportedWithTheReason)
{
    respondWithCurrentDocument();
    m_http.failWith("Could not resolve host");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.saveDocument(nlohmann::json::object(), SERVER_TIME); }), "API write failed: Could not resolve host");
}

TEST_F(CloudStorageClientTest, SaveAfterSaveReusesTheReturnedEtagWithoutReading)
{
    respondWithCurrentDocument();
    m_http.respondWithHeaders(200, documentResponse(nlohmann::json{ { "a", 1 } }), { { "etag", "\"hash-6\"" } });
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 2 } }));

    const auto [first, firstConflict] = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);
    const auto [second, secondConflict] = m_client.saveDocument(nlohmann::json{ { "a", 2 } }, first.updatedAt);

    EXPECT_FALSE(secondConflict);
    ASSERT_EQ(m_http.requests.size(), 3U);
    EXPECT_EQ(m_http.requests.back().method, Method::Put);
    EXPECT_EQ(m_http.requests.back().headers.at("If-Match"), "\"hash-6\"");
}

TEST_F(CloudStorageClientTest, SaveAfterLoadUsesTheLoadedEtagWithoutReading)
{
    respondWithCurrentDocument("\"hash-7\"");
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    std::ignore = m_client.loadDocument();
    std::ignore = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 2U);
    EXPECT_EQ(m_http.requests.back().method, Method::Put);
    EXPECT_EQ(m_http.requests.back().headers.at("If-Match"), "\"hash-7\"");
}

TEST_F(CloudStorageClientTest, KnownEtagIsNotUsedForADifferentBaseline)
{
    respondWithCurrentDocument("\"hash-7\"");
    respondWithCurrentDocument("\"hash-7\"");

    std::ignore = m_client.loadDocument();
    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "local", true } }, SERVER_TIME - 1h);

    EXPECT_TRUE(conflict);
    ASSERT_EQ(m_http.requests.size(), 2U);
    EXPECT_EQ(m_http.requests.back().method, Method::Get);
}

TEST_F(CloudStorageClientTest, OutdatedKnownEtagEndsInAConflictFromTheServer)
{
    respondWithCurrentDocument("\"hash-7\"");
    m_http.respondWithHeaders(409, documentResponse(nlohmann::json{ { "server", true } }), { { "etag", "\"hash-8\"" } });

    std::ignore = m_client.loadDocument();
    const auto [document, conflict] = m_client.saveDocument(nlohmann::json{ { "local", true } }, SERVER_TIME);

    EXPECT_TRUE(conflict);
    EXPECT_EQ(document.etag, "\"hash-8\"");
    EXPECT_EQ(m_http.requests.size(), 2U);
}

TEST_F(CloudStorageClientTest, KnownEtagBelongsToOneUrlOnly)
{
    respondWithCurrentDocument("\"main-hash\"");
    respondWithCurrentDocument("\"partition-hash\"");
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "year", 2025 } }));

    std::ignore = m_client.loadDocument();
    std::ignore = m_client.savePartition("2025", nlohmann::json{ { "year", 2025 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 3U);
    EXPECT_EQ(m_http.requests[1].url, PARTITIONS_URL + "/2025");
    EXPECT_EQ(m_http.requests[1].method, Method::Get);
    EXPECT_EQ(m_http.requests.back().headers.at("If-Match"), "\"partition-hash\"");
}

TEST_F(CloudStorageClientTest, MissingDocumentForgetsTheKnownEtag)
{
    respondWithCurrentDocument("\"hash-7\"");
    m_http.respondWith(404);
    m_http.respondWith(404);
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "a", 1 } }));

    std::ignore = m_client.loadDocument();
    EXPECT_FALSE(m_client.loadDocument().has_value());
    std::ignore = m_client.saveDocument(nlohmann::json{ { "a", 1 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 4U);
    EXPECT_EQ(m_http.requests[2].method, Method::Get);
    EXPECT_EQ(m_http.requests.back().method, Method::Put);
    EXPECT_FALSE(m_http.requests.back().headers.contains("If-Match"));
}

TEST_F(CloudStorageClientTest, DeletedPartitionForgetsTheKnownEtag)
{
    respondWithCurrentDocument("\"hash-7\"");
    m_http.respondWith(204);
    m_http.respondWith(404);
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "year", 2025 } }));

    std::ignore = m_client.loadPartition("2025");
    EXPECT_TRUE(m_client.deletePartition("2025"));
    std::ignore = m_client.savePartition("2025", nlohmann::json{ { "year", 2025 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 4U);
    EXPECT_EQ(m_http.requests[2].method, Method::Get);
    EXPECT_FALSE(m_http.requests.back().headers.contains("If-Match"));
}

TEST_F(CloudStorageClientTest, PartitionListIsReadFromThePartitionsUrl)
{
    m_http.respondWith(200, R"({"partitions":[{"partition":"2025","sizeBytes":120,"contentHash":"abc","updatedAt":"2026-01-15T10:30:00Z"}]})");

    const auto partitions = m_client.listPartitions();

    EXPECT_EQ(m_http.requests.front().method, Method::Get);
    EXPECT_EQ(m_http.requests.front().url, PARTITIONS_URL);
    ASSERT_EQ(partitions.size(), 1U);
    EXPECT_EQ(partitions.front().partition, "2025");
    EXPECT_EQ(partitions.front().sizeBytes, 120);
    EXPECT_EQ(partitions.front().contentHash, "abc");
    EXPECT_EQ(partitions.front().updatedAt, SERVER_TIME);
}

TEST_F(CloudStorageClientTest, MissingPartitionListIsEmpty)
{
    m_http.respondWith(404);

    EXPECT_TRUE(m_client.listPartitions().empty());
}

TEST_F(CloudStorageClientTest, UnexpectedStatusOnPartitionListIsReported)
{
    m_http.respondWith(503);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.listPartitions(); }), "API partition list failed with status 503");
}

TEST_F(CloudStorageClientTest, PartitionIsLoadedFromItsPercentEncodedUrl)
{
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "year", 2025 } }));

    const auto document = m_client.loadPartition("2025 q1");

    EXPECT_EQ(m_http.requests.front().url, PARTITIONS_URL + "/2025%20q1");
    ASSERT_TRUE(document.has_value());
    EXPECT_EQ(document->content, (nlohmann::json{ { "year", 2025 } }));
}

TEST_F(CloudStorageClientTest, MissingPartitionLoadsAsNullopt)
{
    m_http.respondWith(404);

    EXPECT_FALSE(m_client.loadPartition("2025").has_value());
}

TEST_F(CloudStorageClientTest, PartitionIsSavedWithAPutToItsUrl)
{
    respondWithCurrentDocument("\"hash-5\"");
    m_http.respondWith(200, documentResponse(nlohmann::json{ { "year", 2025 } }));

    const auto [document, conflict] = m_client.savePartition("2025", nlohmann::json{ { "year", 2025 } }, SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 2U);
    EXPECT_EQ(m_http.requests.front().method, Method::Get);
    EXPECT_EQ(m_http.requests.front().url, PARTITIONS_URL + "/2025");
    EXPECT_EQ(m_http.requests.back().method, Method::Put);
    EXPECT_EQ(m_http.requests.back().url, PARTITIONS_URL + "/2025");
    EXPECT_EQ(m_http.requests.back().headers.at("If-Match"), "\"hash-5\"");
    EXPECT_FALSE(conflict);
}

TEST_F(CloudStorageClientTest, ConflictingPartitionSaveReturnsTheServerVersion)
{
    respondWithCurrentDocument();
    m_http.respondWith(409, documentResponse(nlohmann::json{ { "server", true } }));

    const auto [document, conflict] = m_client.savePartition("2025", nlohmann::json::object(), SERVER_TIME);

    EXPECT_TRUE(conflict);
}

TEST_F(CloudStorageClientTest, DeletedPartitionIsReportedAsDeleted)
{
    m_http.respondWith(204);

    EXPECT_TRUE(m_client.deletePartition("2025"));
    EXPECT_EQ(m_http.requests.front().method, Method::Delete);
    EXPECT_EQ(m_http.requests.front().url, PARTITIONS_URL + "/2025");
}

TEST_F(CloudStorageClientTest, MissingPartitionIsNotDeleted)
{
    m_http.respondWith(404);

    EXPECT_FALSE(m_client.deletePartition("2025"));
}

TEST_F(CloudStorageClientTest, UnexpectedStatusOnPartitionDeleteIsReported)
{
    m_http.respondWith(500);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.deletePartition("2025"); }), "API partition delete failed with status 500");
}

TEST_F(CloudStorageClientTest, TransportFailureOnPartitionDeleteIsReportedWithTheReason)
{
    m_http.failWith("Connection refused");

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.deletePartition("2025"); }), "API delete failed: Connection refused");
}

TEST_F(CloudStorageClientTest, RateLimitIsReportedWithTheRequestedWait)
{
    m_http.respondWithRetryAfter(429, 45s);

    try
    {
        std::ignore = m_client.loadDocument();
        FAIL() << "Expected CloudRateLimitedException";
    }
    catch (const CloudRateLimitedException& exception)
    {
        EXPECT_EQ(exception.retryAfter(), std::optional{ 45s });
        EXPECT_STREQ(exception.what(), "API rate limit exceeded, retry in 45 s");
    }
}

TEST_F(CloudStorageClientTest, RateLimitedAnswerCarriesTheReportedAllowance)
{
    m_http.respondWithHeaders(429, "", { { "x-ratelimit-limit", "600" }, { "x-ratelimit-remaining", "0" }, { "x-ratelimit-reset", "30" } });

    try
    {
        std::ignore = m_client.loadDocument();
        FAIL() << "Expected CloudRateLimitedException";
    }
    catch (const CloudRateLimitedException& exception)
    {
        EXPECT_EQ(exception.rateLimit(), std::optional(stapik::cloud::RateLimitState{ .limit = 600, .remaining = 0, .resetAfter = 30s }));
    }
}

TEST_F(CloudStorageClientTest, RateLimitWithoutRetryAfterHasNoRequestedWait)
{
    m_http.respondWith(429);

    try
    {
        std::ignore = m_client.saveDocument(nlohmann::json::object(), {});
        FAIL() << "Expected CloudRateLimitedException";
    }
    catch (const CloudRateLimitedException& exception)
    {
        EXPECT_FALSE(exception.retryAfter().has_value());
    }
}

TEST_F(CloudStorageClientTest, ExpiryOfTheApiKeyIsPassedToTheMonitor)
{
    const auto monitor = std::make_shared<stapik::cloud::ApiKeyExpiryMonitor>();
    const CloudStorageClient client(CONFIG, "budget", m_http, monitor);
    m_http.respondWithHeaders(404, "", { { "x-api-key-expires-at", "2026-11-01T08:00:00Z" } });

    EXPECT_FALSE(client.loadDocument().has_value());

    EXPECT_EQ(monitor->expiresAt(), std::optional{ std::chrono::sys_days{ std::chrono::year{ 2026 } / std::chrono::November / 1 } + 8h });
}

TEST_F(CloudStorageClientTest, ChangeFeedIsReadFromTheApiRoot)
{
    m_http.respondWith(200, R"({"serverTime":"2026-01-15T10:31:00Z","changes":[{"slotKey":"budget","sizeBytes":3,"contentHash":"h","updatedAt":"2026-01-15T10:30:30Z","deleted":false}]})");

    const auto list = m_client.listChanges(SERVER_TIME);

    ASSERT_EQ(m_http.requests.size(), 1U);
    EXPECT_EQ(m_http.requests.front().method, Method::Get);
    EXPECT_EQ(m_http.requests.front().url, "https://cloud.example/api/v1/changes?since=2026-01-15T10%3A30%3A00.000000Z");
    EXPECT_EQ(m_http.requests.front().headers.at("x-api-key"), "secret-key");
    ASSERT_EQ(list.changes.size(), 1U);
    EXPECT_EQ(list.changes.front().slotKey, "budget");
}

TEST_F(CloudStorageClientTest, UnexpectedStatusOnChangeFeedIsReported)
{
    m_http.respondWith(400);

    EXPECT_EQ(thrownMessage<CloudStorageException>([&] { std::ignore = m_client.listChanges(SERVER_TIME); }), "API change list failed with status 400");
}

TEST_F(CloudStorageClientTest, UnconfiguredClientHasNoChanges)
{
    const CloudStorageClient unconfigured(CloudStorageConfig{ .apiUrl = "https://cloud.example", .apiKey = {} }, "budget", m_http);

    const auto list = unconfigured.listChanges(SERVER_TIME);

    EXPECT_TRUE(list.changes.empty());
    EXPECT_EQ(list.serverTime, SERVER_TIME);
    EXPECT_TRUE(m_http.requests.empty());
}
