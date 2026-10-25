#include "CloudAssetClient.hpp"
#include "CloudAssetProtocol.hpp"

#include "stapik/log/Log.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <mutex>
#include <tuple>
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
        constexpr int HTTP_TOO_MANY_REQUESTS = 429;
        constexpr int HTTP_PAYLOAD_TOO_LARGE = 413;
        constexpr int HTTP_UNSUPPORTED_MEDIA_TYPE = 415;
        constexpr int HTTP_UNPROCESSABLE_CONTENT = 422;
        constexpr int HTTP_CONFLICT = 409;
        // Interruptions of a chunked upload in a row, without a chunk getting through in between, that are still worked around.
        constexpr int MAX_UPLOAD_INTERRUPTIONS = 3;
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

    CloudAssetClient::CloudAssetClient(CloudStorageConfig config, std::string slotKey, const http::IHttpClient& httpClient, const AssetChunking chunking,
                                     std::shared_ptr<ApiKeyExpiryMonitor> expiryMonitor) :
        m_config(std::move(config)),
        m_slotKey(std::move(slotKey)),
        m_http(httpClient),
        m_chunking(chunking),
        m_expiryMonitor(std::move(expiryMonitor))
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

        if (m_expiryMonitor)
            m_expiryMonitor->observe(result.value());

        if (result.value().status == HTTP_TOO_MANY_REQUESTS)
            throw CloudRateLimitedException(result.value().retryAfter, parseRateLimit(result.value()));

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
        const auto checksum = sha256Hex(content);
        auto uploadUrl = pendingUploadUrl(filename, checksum);

        try
        {
            std::optional<Resumption> resumption;
            if (uploadUrl)
            {
                resumption = findUpload(*uploadUrl, filename, content.size(), checksum, hooks);
                if (!resumption)
                {
                    forgetPendingUpload(filename);
                    uploadUrl.reset();
                }
            }

            if (resumption && resumption->stored)
            {
                forgetPendingUpload(filename);
                return *resumption->stored;
            }

            if (!uploadUrl)
            {
                uploadUrl = startUpload(filename, mimeType, content.size(), checksum, hooks);
                rememberPendingUpload(filename, checksum, *uploadUrl);
            }

            auto asset = sendChunks(*uploadUrl, filename, content, checksum, hooks, resumption ? resumption->offset : 0);
            forgetPendingUpload(filename);
            return asset;
        }
        catch (const CloudRateLimitedException&)
        {
            throw;
        }
        catch (...)
        {
            forgetPendingUpload(filename);
            if (uploadUrl)
                abortUpload(*uploadUrl);
            throw;
        }
    }

    std::string CloudAssetClient::startUpload(const std::string& filename, const std::string& mimeType, const std::size_t totalBytes, const std::string& checksum,
                                              const AssetUploadHooks& hooks) const
    {
        const auto start = perform(http::Method::Post, cloud::assetUploadsUrl(m_config.normalizedApiUrl()),
                                   http::toBytes(buildAssetUploadRequest(m_slotKey, filename, static_cast<std::int64_t>(totalBytes), mimeType, checksum)),
                                   "API asset upload failed: ", {}, hooks.isCancelled, { { "Content-Type", "application/json" } });

        if (start.status == HTTP_PAYLOAD_TOO_LARGE)
            throw AssetTooLargeException(std::format("File '{}' is too large for the cloud slot", filename));

        if (start.status != HTTP_CREATED)
            throw CloudStorageException(std::format("API asset upload failed with status {}", start.status));

        return cloud::assetUploadUrl(m_config.normalizedApiUrl(), parseAssetUploadState(http::toText(start.body)).uploadId);
    }

    AssetInfo CloudAssetClient::sendChunks(const std::string& uploadUrl, const std::string& filename, const AssetBytes& content, const std::string& checksum,
                                           const AssetUploadHooks& hooks, const std::size_t startOffset) const
    {
        const auto totalBytes = content.size();
        int interruptions = 0;

        for (std::size_t offset = startOffset; offset < totalBytes;)
        {
            if (hooks.isCancelled && hooks.isCancelled())
                throw AssetTransferCancelledException("Asset transfer cancelled");

            const auto chunkBytes = std::min(m_chunking.chunkBytes, totalBytes - offset);
            const auto chunkBegin = content.begin() + static_cast<std::ptrdiff_t>(offset);
            http::Bytes chunk(chunkBegin, chunkBegin + static_cast<std::ptrdiff_t>(chunkBytes));

            std::optional<http::Response> response;
            std::string interruption;
            try
            {
                response = perform(http::Method::Patch, uploadUrl, std::move(chunk), "API asset upload failed: ",
                                   chunkProgressReporter(hooks, offset, chunkBytes, totalBytes), hooks.isCancelled,
                                   { { "Content-Type", "application/octet-stream" }, { "Upload-Offset", std::to_string(offset) } });
            }
            catch (const AssetTransferCancelledException&)
            {
                throw;
            }
            catch (const CloudRateLimitedException&)
            {
                throw;
            }
            catch (const CloudStorageException& exception)
            {
                interruption = exception.what();
            }

            if (response)
            {
                const auto& [httpStatus, body, retryAfter, headers] = *response;

                if (httpStatus == HTTP_OK)
                {
                    const auto state = parseAssetUploadState(http::toText(body));
                    offset += chunkBytes;

                    if (state.asset)
                        return *state.asset;

                    if (state.offset != static_cast<std::int64_t>(offset))
                        throw CloudStorageException("The cloud acknowledged an unexpected part of the upload");

                    interruptions = 0;
                    continue;
                }

                if (httpStatus == HTTP_PAYLOAD_TOO_LARGE)
                    throw CloudStorageException(std::format("API asset upload failed: the server accepts chunks smaller than {} bytes", m_chunking.chunkBytes));

                throwIfFileRefused(httpStatus, body, filename);

                // 409: the cloud is at another offset than assumed. 404: the upload is gone, perhaps because the last chunk completed it.
                if (httpStatus != HTTP_CONFLICT && httpStatus != HTTP_NOT_FOUND)
                    throw CloudStorageException(std::format("API asset upload failed with status {}", httpStatus));

                interruption = std::format("API asset upload failed with status {}", httpStatus);
            }

            if (++interruptions > MAX_UPLOAD_INTERRUPTIONS)
                throw CloudStorageException(interruption);

            log::info("The upload of '{}' was interrupted ({}), asking the cloud how much of it arrived", filename, interruption);

            const auto [resumedOffset, stored] = resumeUpload(uploadUrl, filename, totalBytes, checksum, hooks);
            if (stored)
                return *stored;

            offset = resumedOffset;
        }

        throw CloudStorageException("The cloud did not confirm that the upload is complete");
    }

    auto CloudAssetClient::resumeUpload(const std::string& uploadUrl, const std::string& filename, const std::size_t totalBytes, const std::string& checksum,
                                        const AssetUploadHooks& hooks) const -> Resumption
    {
        if (const auto found = findUpload(uploadUrl, filename, totalBytes, checksum, hooks))
            return *found;

        throw CloudStorageException(std::format("The cloud lost the upload of '{}'", filename));
    }

    auto CloudAssetClient::findUpload(const std::string& uploadUrl, const std::string& filename, const std::size_t totalBytes, const std::string& checksum,
                                      const AssetUploadHooks& hooks) const -> std::optional<Resumption>
    {
        const auto [httpStatus, body, retryAfter, headers] = perform(http::Method::Get, uploadUrl, http::NoBody{}, "API asset upload failed: ", {}, hooks.isCancelled);

        if (httpStatus == HTTP_OK)
        {
            const auto state = parseAssetUploadState(http::toText(body));
            if (state.offset < 0 || state.offset > static_cast<std::int64_t>(totalBytes))
                throw CloudStorageException("The cloud reported an invalid offset of the upload");

            return Resumption{ .offset = static_cast<std::size_t>(state.offset) };
        }

        if (httpStatus != HTTP_NOT_FOUND)
            throw CloudStorageException(std::format("API asset upload failed with status {}", httpStatus));

        // The upload is gone. If the last chunk completed it before the answer was lost, the file is already stored.
        const auto sameChecksum = [&checksum](const std::string& other)
        {
            return std::ranges::equal(checksum, other, [](const char left, const char right)
            {
                return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
            });
        };

        for (const auto& asset : listAssets())
        {
            if (asset.filename == filename && asset.sizeBytes == static_cast<std::int64_t>(totalBytes) && sameChecksum(asset.checksumSha256))
                return Resumption{ .stored = asset };
        }

        return std::nullopt;
    }

    void CloudAssetClient::abortUpload(const std::string& uploadUrl) const
    {
        try
        {
            std::ignore = perform(http::Method::Delete, uploadUrl, http::NoBody{}, "API asset upload failed: ");
        }
        catch (const CloudStorageException& exception)
        {
            log::debug("Cannot abort the upload in the cloud: {}", exception.what());
        }
    }

    std::optional<std::string> CloudAssetClient::pendingUploadUrl(const std::string& filename, const std::string& checksum) const
    {
        std::optional<PendingUpload> pending;
        {
            const std::lock_guard lock(m_pendingUploadsMutex);
            if (const auto found = m_pendingUploads.find(filename); found != m_pendingUploads.end())
                pending = found->second;
        }

        if (!pending)
            return std::nullopt;

        if (pending->checksum == checksum)
            return pending->uploadUrl;

        forgetPendingUpload(filename);
        abortUpload(pending->uploadUrl);
        return std::nullopt;
    }

    void CloudAssetClient::rememberPendingUpload(const std::string& filename, const std::string& checksum, const std::string& uploadUrl) const
    {
        const std::lock_guard lock(m_pendingUploadsMutex);
        m_pendingUploads.insert_or_assign(filename, PendingUpload{ .checksum = checksum, .uploadUrl = uploadUrl });
    }

    void CloudAssetClient::forgetPendingUpload(const std::string& filename) const
    {
        const std::lock_guard lock(m_pendingUploadsMutex);
        m_pendingUploads.erase(filename);
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
