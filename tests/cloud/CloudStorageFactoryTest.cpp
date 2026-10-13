#include "stapik/cloud/CloudStorageFactory.hpp"

#include "stapik/cloud/CompressedCloudStorage.hpp"

#include "support/FakeHttpClient.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>
#include <tuple>

TEST(CloudStorageFactoryTest, PlainConfigCreatesCompressedStorage)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key" };

    const auto storage = stapik::cloud::createCloudStorage(config, "budget");

    EXPECT_NE(dynamic_cast<stapik::cloud::CompressedCloudStorage*>(storage.get()), nullptr);
}

TEST(CloudStorageFactoryTest, EncryptedConfigCreatesCompressedStorageToo)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key", .encryptionPassphrase = "passphrase" };

    const auto storage = stapik::cloud::createCloudStorage(config, "budget");

    EXPECT_NE(dynamic_cast<stapik::cloud::CompressedCloudStorage*>(storage.get()), nullptr);
}

TEST(CloudStorageFactoryTest, CreatedStorageSendsItsRequestsThroughTheGivenHttpClient)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key" };
    stapik::test::FakeHttpClient http;
    http.respondWith(404);

    const auto storage = stapik::cloud::createCloudStorage(config, "budget", http);
    EXPECT_FALSE(storage->loadDocument().has_value());

    ASSERT_EQ(http.requests.size(), 1U);
    EXPECT_EQ(http.requests.front().url, "https://cloud.example/api/v1/documents/budget");
}

namespace
{
    // What the cloud admin panel looks at to show an end-to-end encrypted document as read-only: the top level of the
    // content string that travels in the PUT body.
    nlohmann::json contentSentToTheServer(const stapik::test::FakeHttpClient& http)
    {
        const auto& put = http.requests.back();
        EXPECT_EQ(put.method, stapik::http::Method::Put);
        return nlohmann::json::parse(nlohmann::json::parse(stapik::test::bodyText(put)).at("content").get<std::string>());
    }

    const std::string SAVED_DOCUMENT = R"({"content":"{}","updatedAt":"2026-01-15T10:30:00Z"})";
}

TEST(CloudStorageFactoryTest, EncryptedDocumentIsMarkedAsEndToEndEncryptedOnTheWire)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key", .encryptionPassphrase = "passphrase" };
    stapik::test::FakeHttpClient http;
    http.respondWith(404);
    http.respondWith(200, SAVED_DOCUMENT);

    const auto storage = stapik::cloud::createCloudStorage(config, "budget", http);
    std::ignore = storage->saveDocument(nlohmann::json{ { "title", "secret" } }, {});

    const auto sent = contentSentToTheServer(http);
    EXPECT_EQ(sent.at("format"), "stapik-e2e");
    EXPECT_EQ(sent.dump().find("secret"), std::string::npos);
}

TEST(CloudStorageFactoryTest, EncryptedPartitionIsMarkedAsEndToEndEncryptedOnTheWire)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key", .encryptionPassphrase = "passphrase" };
    stapik::test::FakeHttpClient http;
    http.respondWith(404);
    http.respondWith(200, SAVED_DOCUMENT);

    const auto storage = stapik::cloud::createCloudStorage(config, "budget", http);
    std::ignore = storage->savePartition("2025", nlohmann::json{ { "title", "secret" } }, {});

    EXPECT_EQ(contentSentToTheServer(http).at("format"), "stapik-e2e");
}

TEST(CloudStorageFactoryTest, UnencryptedDocumentIsNotMarkedAsEndToEndEncrypted)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key" };
    stapik::test::FakeHttpClient http;
    http.respondWith(404);
    http.respondWith(200, SAVED_DOCUMENT);

    const auto storage = stapik::cloud::createCloudStorage(config, "budget", http);
    std::ignore = storage->saveDocument(nlohmann::json{ { "title", "plain" } }, {});

    EXPECT_NE(contentSentToTheServer(http).value("format", ""), "stapik-e2e");
}
