#include "stapik/cloud/CloudStorageFactory.hpp"

#include "stapik/cloud/CloudStorageClient.hpp"
#include "stapik/cloud/EncryptedCloudStorage.hpp"

#include <gtest/gtest.h>

TEST(CloudStorageFactoryTest, PlainConfigCreatesTheClient)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key" };

    const auto storage = stapik::cloud::createCloudStorage(config, "budget");

    EXPECT_NE(dynamic_cast<CloudStorageClient*>(storage.get()), nullptr);
}

TEST(CloudStorageFactoryTest, PassphraseWrapsTheClientInEncryption)
{
    const CloudStorageConfig config{ .apiUrl = "https://cloud.example", .apiKey = "key", .encryptionPassphrase = "passphrase" };

    const auto storage = stapik::cloud::createCloudStorage(config, "budget");

    EXPECT_NE(dynamic_cast<stapik::cloud::EncryptedCloudStorage*>(storage.get()), nullptr);
}
