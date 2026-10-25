#pragma once

#include "CloudStorageConfig.hpp"
#include "ApiKeyExpiry.hpp"
#include "AssetSizeLimit.hpp"
#include "IAssetStorage.hpp"

#include "stapik/http/IHttpClient.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace stapik::cloud
{
    // Files from thresholdBytes up are sent in resumable chunks of chunkBytes (/asset-uploads) instead of one
    // multipart request. The server accepts chunks of 64 MiB by default, its operator can lower that.
    struct AssetChunking
    {
        static constexpr std::size_t DEFAULT_THRESHOLD_BYTES = 32U * 1024U * 1024U;
        static constexpr std::size_t DEFAULT_CHUNK_BYTES = 8U * 1024U * 1024U;

        std::size_t thresholdBytes = DEFAULT_THRESHOLD_BYTES;
        std::size_t chunkBytes = DEFAULT_CHUNK_BYTES;
    };

    // Client of a BINARY_COLLECTION slot of Stapik Cloud (/api/v1/assets/<slotKey>). It uses the same
    // configuration (address and API key) as CloudStorageClient, only the slot differs.
    class CloudAssetClient final : public IAssetStorage
    {
    public:
        CloudAssetClient(CloudStorageConfig config, std::string slotKey, const http::IHttpClient& httpClient = http::defaultHttpClient(), AssetChunking chunking = {},
                         std::shared_ptr<ApiKeyExpiryMonitor> expiryMonitor = {}, AssetSizeLimit slotLimit = {});

        [[nodiscard]] std::vector<AssetInfo> listAssets() const override;
        [[nodiscard]] std::optional<AssetBytes> downloadAsset(const std::string& filename) const override;
        [[nodiscard]] bool deleteAsset(const std::string& filename) const override;
        using IAssetStorage::uploadAsset;
        [[nodiscard]] AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const override;
    private:
        [[nodiscard]] http::Response perform(http::Method method, const std::string& url, http::RequestBody body, const char* failureLabel,
                                             http::ProgressCallback onProgress = {}, http::CancelCheck isCancelled = {},
                                             std::map<std::string, std::string> extraHeaders = {}) const;
        [[nodiscard]] std::string assetsUrl() const;
        [[nodiscard]] std::string assetUrl(const std::string& filename) const;
        // What the cloud holds of an interrupted chunked upload: how many bytes arrived, or the finished file when the
        // last chunk completed the upload before the answer was lost.
        struct Resumption
        {
            std::size_t offset = 0;
            std::optional<AssetInfo> stored;
        };

        struct PendingUpload
        {
            std::string checksum;
            std::string uploadUrl;
        };

        [[nodiscard]] AssetInfo uploadInChunks(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const;
        [[nodiscard]] std::string startUpload(const std::string& filename, const std::string& mimeType, std::size_t totalBytes, const std::string& checksum,
                                              const AssetUploadHooks& hooks) const;
        [[nodiscard]] AssetInfo sendChunks(const std::string& uploadUrl, const std::string& filename, const AssetBytes& content, const std::string& checksum,
                                           const AssetUploadHooks& hooks, std::size_t startOffset) const;
        [[nodiscard]] std::optional<Resumption> findUpload(const std::string& uploadUrl, const std::string& filename, std::size_t totalBytes, const std::string& checksum,
                                                           const AssetUploadHooks& hooks) const;
        [[nodiscard]] Resumption resumeUpload(const std::string& uploadUrl, const std::string& filename, std::size_t totalBytes, const std::string& checksum,
                                              const AssetUploadHooks& hooks) const;
        void abortUpload(const std::string& uploadUrl) const;
        [[nodiscard]] std::optional<std::string> pendingUploadUrl(const std::string& filename, const std::string& checksum) const;
        void rememberPendingUpload(const std::string& filename, const std::string& checksum, const std::string& uploadUrl) const;
        void forgetPendingUpload(const std::string& filename) const;

        CloudStorageConfig m_config;
        std::string m_slotKey;
        const http::IHttpClient& m_http;
        AssetChunking m_chunking;
        std::shared_ptr<ApiKeyExpiryMonitor> m_expiryMonitor;
        std::size_t m_maxResponseBytes;
        mutable std::mutex m_pendingUploadsMutex;
        mutable std::map<std::string, PendingUpload> m_pendingUploads;
    };
}
