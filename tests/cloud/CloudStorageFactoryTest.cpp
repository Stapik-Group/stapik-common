#include "stapik/cloud/CloudStorageFactory.hpp"

#include "stapik/cloud/CompressedCloudStorage.hpp"

#include "support/FakeHttpClient.hpp"

#include <gtest/gtest.h>

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
