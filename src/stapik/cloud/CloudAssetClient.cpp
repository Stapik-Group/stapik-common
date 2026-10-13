#include "CloudAssetClient.hpp"
#include "CloudAssetProtocol.hpp"

#include "stapik/log/Log.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace stapik::cloud
{
    namespace
    {
        constexpr std::size_t MAX_RESPONSE_BYTES = 64U * 1024U * 1024U;
        constexpr int HTTP_OK = 200;
        constexpr int HTTP_CREATED = 201;
        constexpr int HTTP_NO_CONTENT = 204;
        constexpr int HTTP_NOT_FOUND = 404;
        constexpr int HTTP_PAYLOAD_TOO_LARGE = 413;
        constexpr int HTTP_UNSUPPORTED_MEDIA_TYPE = 415;
        constexpr int HTTP_UNPROCESSABLE_CONTENT = 422;
    }

    namespace
    {
        // The statuses by which the server refuses a file as a whole, whichever way it was sent.
        void throwIfFileRefused(const int httpStatus, const http::Bytes& body, const std::string& filename)
        {
            if (httpStatus == HTTP_PAYLOAD_TOO_LARGE)
                throw AssetTooLargeException(std::format("File '{}' is too large for the cloud slot", filename));

            if (httpStatus == HTTP_UNSUPPORTED_MEDIA_TYPE)
                throw AssetTypeNotAllowedException(parseProblemDetail(http::toText(body)).value_or(std::format("File '{}' has a type the cloud slot does not accept", filename)));

            if (httpStatus == HTTP_UNPROCESSABLE_CONTENT)
                throw AssetChecksumMismatchException(parseProblemDetail(http::toText(body)).value_or(std::format("File '{}' was damaged on the way to the cloud", filename)));
        }

        http::ProgressCallback chunkProgressReporter(const AssetUploadHooks& hooks, const std::uint64_t sentBefore, const std::uint64_t chunkBytes, const std::uint64_t totalBytes)
        {
            if (!hooks.onProgress)
                return {};

            return [onProgress = hooks.onProgress, sentBefore, chunkBytes, totalBytes](const http::TransferProgress& progress)
            {
                onProgress(AssetUploadProgress{ .sentBytes = sentBefore + std::min(progress.uploadedBytes, chunkBytes), .totalBytes = totalBytes });
            };
        }

        http::ProgressCallback uploadProgressReporter(const AssetUploadHooks& hooks, const std::uint64_t totalBytes)
        {
            if (!hooks.onProgress)
                return {};

            return [onProgress = hooks.onProgress, totalBytes](const http::TransferProgress& progress)
            {
                onProgress(AssetUploadProgress{ .sentBytes = std::min(progress.uploadedBytes, totalBytes), .totalBytes = totalBytes });
            };
        }
    }

    CloudAssetClient::CloudAssetClient(CloudStorageConfig config, std::string slotKey, const http::IHttpClient& httpClient, const AssetChunking chunking) :
        m_config(std::move(config)),
        m_slotKey(std::move(slotKey)),
        m_http(httpClient),
        m_chunking(chunking)
    {
        m_chunking.chunkBytes = std::max<std::size_t>(m_chunking.chunkBytes, 1);

        if (m_config.isConfigured() && !m_config.isSecure())
            log::warning("Cloud API URL does not use https:// - the API key and files are sent unencrypted. This is not a secure connection.");
    }

    std::string CloudAssetClient::assetsUrl() const
    {
        return cloud::assetsUrl(m_config.normalizedApiUrl(), m_slotKey);
    }

    std::string CloudAssetClient::assetUrl(const std::string& filename) const
    {
        return cloud::assetUrl(m_config.normalizedApiUrl(), m_slotKey, normalizeAssetFilename(filename));
    }

    http::Response CloudAssetClient::perform(const http::Method method, const std::string& url, http::RequestBody body, const char* failureLabel,
                                             http::ProgressCallback onProgress, http::CancelCheck isCancelled,
                                             std::map<std::string, std::string> extraHeaders) const
    {
        http::Request request{
            .method = method,
            .url = url,
            .headers = { { "x-api-key", m_config.apiKey } },
            .body = std::move(body),
            .timeout = http::NO_TIMEOUT,
            .maxResponseBytes = MAX_RESPONSE_BYTES,
            .onProgress = std::move(onProgress),
            .isCancelled = std::move(isCancelled)
        };
        request.headers.merge(std::move(extraHeaders));

        auto result = m_http.send(request);
        if (!result)
        {
            if (result.error().cancelled)
                throw AssetTransferCancelledException("Asset transfer cancelled");

            throw CloudStorageException(std::string(failureLabel) + result.error().message);
        }

        return std::move(result).value();
    }

    std::vector<AssetInfo> CloudAssetClient::listAssets() const
    {
        if (!m_config.isConfigured())
            return {};

        const auto [httpStatus, body, retryAfter, headers] = perform(http::Method::Get, assetsUrl(), http::NoBody{}, "API asset list failed: ");

        if (httpStatus == HTTP_NOT_FOUND)
            return {};

        if (httpStatus != HTTP_OK)
            throw CloudStorageException(std::format("API asset list failed with status {}", httpStatus));

        return parseAssetList(http::toText(body));
    }

    std::optional<AssetBytes> CloudAssetClient::downloadAsset(const std::string& filename) const
    {
        if (!m_config.isConfigured())
            return std::nullopt;

        auto [httpStatus, body, retryAfter, headers] = perform(http::Method::Get, assetUrl(filename), http::NoBody{}, "API asset download failed: ");

        if (httpStatus == HTTP_NOT_FOUND)
            return std::nullopt;

        if (httpStatus != HTTP_OK)
            throw CloudStorageException(std::format("API asset download failed with status {}", httpStatus));

        return std::move(body);
    }

    AssetInfo CloudAssetClient::uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const
    {
        if (!m_config.isConfigured())
            throw CloudStorageException("Cloud sync is not configured");

        const auto storedFilename = normalizeAssetFilename(filename);
        if (!content.empty() && content.size() >= m_chunking.thresholdBytes)
            return uploadInChunks(storedFilename, mimeType, content, hooks);

        http::MultipartFile file{ .fieldName = "file", .filename = storedFilename, .mimeType = mimeType, .content = content };
        const auto [httpStatus, body, retryAfter, headers] = perform(http::Method::Put, assetUrl(storedFilename), std::move(file), "API asset upload failed: ",
                                                uploadProgressReporter(hooks, content.size()), hooks.isCancelled,
                                                { { "X-Checksum-Sha256", sha256Hex(content) } });

        throwIfFileRefused(httpStatus, body, filename);

        if (httpStatus != HTTP_OK && httpStatus != HTTP_CREATED)
            throw CloudStorageException(std::format("API asset upload failed with status {}", httpStatus));

        return parseAssetMetadata(http::toText(body));
    }

    AssetInfo CloudAssetClient::uploadInChunks(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const
    {
        const auto totalBytes = content.size();

        const auto start = perform(http::Method::Post, cloud::assetUploadsUrl(m_config.normalizedApiUrl()),
                                   http::toBytes(buildAssetUploadRequest(m_slotKey, filename, static_cast<std::int64_t>(totalBytes), mimeType, sha256Hex(content))),
                                   "API asset upload failed: ", {}, hooks.isCancelled, { { "Content-Type", "application/json" } });

        if (start.status == HTTP_PAYLOAD_TOO_LARGE)
            throw AssetTooLargeException(std::format("File '{}' is too large for the cloud slot", filename));

        if (start.status != HTTP_CREATED)
            throw CloudStorageException(std::format("API asset upload failed with status {}", start.status));

        const auto uploadUrl = cloud::assetUploadUrl(m_config.normalizedApiUrl(), parseAssetUploadState(http::toText(start.body)).uploadId);

        for (std::size_t offset = 0; offset < totalBytes;)
        {
            if (hooks.isCancelled && hooks.isCancelled())
                throw AssetTransferCancelledException("Asset transfer cancelled");

            const auto chunkBytes = std::min(m_chunking.chunkBytes, totalBytes - offset);
            const auto chunkBegin = content.begin() + static_cast<std::ptrdiff_t>(offset);
            http::Bytes chunk(chunkBegin, chunkBegin + static_cast<std::ptrdiff_t>(chunkBytes));

            const auto [httpStatus, body, retryAfter, headers] = perform(http::Method::Patch, uploadUrl, std::move(chunk), "API asset upload failed: ",
                                                                         chunkProgressReporter(hooks, offset, chunkBytes, totalBytes), hooks.isCancelled,
                                                                         { { "Content-Type", "application/octet-stream" }, { "Upload-Offset", std::to_string(offset) } });

            if (httpStatus == HTTP_PAYLOAD_TOO_LARGE)
                throw CloudStorageException(std::format("API asset upload failed: the server accepts chunks smaller than {} bytes", m_chunking.chunkBytes));

            throwIfFileRefused(httpStatus, body, filename);

            if (httpStatus != HTTP_OK)
                throw CloudStorageException(std::format("API asset upload failed with status {}", httpStatus));

            const auto state = parseAssetUploadState(http::toText(body));
            offset += chunkBytes;

            if (state.asset)
                return *state.asset;

            if (state.offset != static_cast<std::int64_t>(offset))
                throw CloudStorageException("The cloud acknowledged an unexpected part of the upload");
        }

        throw CloudStorageException("The cloud did not confirm that the upload is complete");
    }

    bool CloudAssetClient::deleteAsset(const std::string& filename) const
    {
        if (!m_config.isConfigured())
            return false;

        const auto [httpStatus, body, retryAfter, headers] = perform(http::Method::Delete, assetUrl(filename), http::NoBody{}, "API asset delete failed: ");

        if (httpStatus == HTTP_NO_CONTENT)
            return true;

        if (httpStatus == HTTP_NOT_FOUND)
            return false;

        throw CloudStorageException(std::format("API asset delete failed with status {}", httpStatus));
    }
}
