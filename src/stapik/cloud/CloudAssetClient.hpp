#pragma once

#include "CloudStorageConfig.hpp"
#include "IAssetStorage.hpp"

#include <string>
#include <vector>

namespace stapik::cloud
{
    // Client of a BINARY_COLLECTION slot of Stapik Cloud (/api/v1/assets/<slotKey>). It uses the same
    // configuration (address and API key) as CloudStorageClient, only the slot differs.
    class CloudAssetClient final : public IAssetStorage
    {
    public:
        CloudAssetClient(CloudStorageConfig config, std::string slotKey);

        [[nodiscard]] std::vector<AssetInfo> listAssets() const override;
        [[nodiscard]] std::optional<AssetBytes> downloadAsset(const std::string& filename) const override;
        AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content) const override;
        [[nodiscard]] bool deleteAsset(const std::string& filename) const override;

    private:
        struct RawResponse
        {
            long httpStatus;
            AssetBytes body;
        };

        struct FilePart
        {
            const std::string& filename;
            const std::string& mimeType;
            const AssetBytes& content;
        };

        [[nodiscard]] RawResponse perform(const std::string& url, const char* method, const FilePart* file, const char* failureLabel) const;
        [[nodiscard]] std::string assetsUrl() const;
        [[nodiscard]] std::string assetUrl(const std::string& filename) const;

        CloudStorageConfig m_config;
        std::string m_slotKey;
    };
}
