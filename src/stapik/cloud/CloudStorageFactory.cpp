#include "CloudStorageFactory.hpp"

#include "CloudStorageClient.hpp"
#include "EncryptedCloudStorage.hpp"

#include <utility>

namespace stapik::cloud
{
    std::unique_ptr<ICloudStorage> createCloudStorage(const CloudStorageConfig& config, const std::string& slotKey)
    {
        auto client = std::make_unique<CloudStorageClient>(config, slotKey);
        if (!config.isEndToEndEncrypted())
            return client;

        return std::make_unique<EncryptedCloudStorage>(std::move(client), config.encryptionPassphrase, slotKey);
    }
}
