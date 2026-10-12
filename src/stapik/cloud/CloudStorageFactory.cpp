#include "CloudStorageFactory.hpp"

#include "CloudStorageClient.hpp"
#include "CompressedCloudStorage.hpp"
#include "EncryptedCloudStorage.hpp"

#include <utility>

namespace stapik::cloud
{
    std::unique_ptr<ICloudStorage> createCloudStorage(const CloudStorageConfig& config, const std::string& slotKey, const http::IHttpClient& httpClient)
    {
        std::unique_ptr<ICloudStorage> storage = std::make_unique<CloudStorageClient>(config, slotKey, httpClient);

        if (config.isEndToEndEncrypted())
            storage = std::make_unique<EncryptedCloudStorage>(std::move(storage), config.encryptionPassphrase, slotKey);

        return std::make_unique<CompressedCloudStorage>(std::move(storage));
    }
}
