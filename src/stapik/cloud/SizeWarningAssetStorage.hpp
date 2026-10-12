#pragma once

#include "AssetSizeLimit.hpp"
#include "IAssetStorage.hpp"

#include <functional>
#include <string>
#include <vector>

namespace stapik::cloud
{
    struct AssetSizeWarning
    {
        std::string filename;
        std::int64_t sizeBytes{};
        AssetSizeLimit limit;
        AssetSizeStatus status{};
    };

    // Reports a file that is near or over the size limit of the slot just before it is uploaded.
    // The upload itself is never blocked: the server stays the authority and still answers with AssetTooLargeException.
    class SizeWarningAssetStorage final : public IAssetStorage
    {
    public:
        using WarningHandler = std::function<void(const AssetSizeWarning&)>;

        SizeWarningAssetStorage(const IAssetStorage& inner, AssetSizeLimit limit, WarningHandler onWarning = {});

        [[nodiscard]] std::vector<AssetInfo> listAssets() const override;
        [[nodiscard]] std::optional<AssetBytes> downloadAsset(const std::string& filename) const override;
        [[nodiscard]] bool deleteAsset(const std::string& filename) const override;
        using IAssetStorage::uploadAsset;
        [[nodiscard]] AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const override;

    private:
        const IAssetStorage& m_inner;
        AssetSizeLimit m_limit;
        WarningHandler m_onWarning;
    };
}
