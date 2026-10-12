#include "stapik/cloud/CloudStorageFactory.hpp"

#include "stapik/cloud/CompressedCloudStorage.hpp"

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
