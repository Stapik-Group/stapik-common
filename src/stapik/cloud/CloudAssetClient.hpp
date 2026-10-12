#pragma once

#include "CloudStorageConfig.hpp"
#include "IAssetStorage.hpp"

#include "stapik/http/IHttpClient.hpp"

#include <string>
#include <vector>

namespace stapik::cloud
{
    // Client of a BINARY_COLLECTION slot of Stapik Cloud (/api/v1/assets/<slotKey>). It uses the same
    // configuration (address and API key) as CloudStorageClient, only the slot differs.
    class CloudAssetClient final : public IAssetStorage
    {
    public:
        CloudAssetClient(CloudStorageConfig config, std::string slotKey, const http::IHttpClient& httpClient = http::defaultHttpClient());

        [[nodiscard]] std::vector<AssetInfo> listAssets() const override;
        [[nodiscard]] std::optional<AssetBytes> downloadAsset(const std::string& filename) const override;
        [[nodiscard]] AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content) const override;
        [[nodiscard]] bool deleteAsset(const std::string& filename) const override;

    private:
        [[nodiscard]] http::Response perform(http::Method method, const std::string& url, http::RequestBody body, const char* failureLabel) const;
        [[nodiscard]] std::string assetsUrl() const;
        [[nodiscard]] std::string assetUrl(const std::string& filename) const;

        CloudStorageConfig m_config;
        std::string m_slotKey;
        const http::IHttpClient& m_http;
    };
}
