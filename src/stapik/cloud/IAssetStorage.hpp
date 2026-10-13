#pragma once

#include "CloudStorageException.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace stapik::cloud
{
    using AssetBytes = std::vector<std::uint8_t>;

    struct AssetInfo
    {
        std::string filename;
        std::string mimeType;
        std::int64_t sizeBytes{};
        std::string checksumSha256;
        std::chrono::system_clock::time_point updatedAt;
    };

    struct AssetUploadProgress
    {
        std::uint64_t sentBytes{};
        std::uint64_t totalBytes{};
    };

    // Both callbacks run on the thread that called uploadAsset(). Returning true from isCancelled aborts the upload.
    struct AssetUploadHooks
    {
        std::function<void(const AssetUploadProgress&)> onProgress;
        std::function<bool()> isCancelled;
    };

    class AssetTransferCancelledException : public CloudStorageException
    {
    public:
        using CloudStorageException::CloudStorageException;
    };

    // The server refuses a file bigger than the maximum file size of the slot (HTTP 413).
    // Uploading the same file again can never succeed, so callers should not retry it.
    class AssetTooLargeException : public CloudStorageException
    {
    public:
        using CloudStorageException::CloudStorageException;
    };

    // The cloud cannot store a file under this name (empty, too long, forbidden characters, a reserved name, ...).
    // The name is checked before any request is sent, with the rules the server applies.
    class InvalidAssetFilenameException : public CloudStorageException
    {
    public:
        using CloudStorageException::CloudStorageException;
    };

    // The server refuses the type of the file, detected from its content, in this slot (HTTP 415).
    // Uploading the same file again can never succeed, so callers should not retry it.
    class AssetTypeNotAllowedException : public CloudStorageException
    {
    public:
        using CloudStorageException::CloudStorageException;
    };

    // The server received different bytes than were sent (HTTP 422, the SHA-256 of the file does not match).
    // The file that was in the cloud before is untouched; uploading the file again may succeed.
    class AssetChecksumMismatchException : public CloudStorageException
    {
    public:
        using CloudStorageException::CloudStorageException;
    };

    // Abstraction of a remote collection of binary files (a BINARY_COLLECTION slot). It allows the
    // logic built on top of it to be tested against a fake without any network access.
    //
    // Contract:
    //  - A file is identified by its filename; uploading under an existing filename overwrites it.
    //  - listAssets() returns an empty list when the slot does not exist or the storage is not
    //    configured; downloadAsset() returns std::nullopt when the file does not exist or the storage
    //    is not configured; deleteAsset() returns false when there was nothing to delete.
    //  - uploadAsset() throws AssetTooLargeException when the file exceeds the size limit of the slot and
    //    AssetTypeNotAllowedException when the slot does not accept the type of the file.
    //  - uploadAsset() throws AssetChecksumMismatchException when the server received damaged data.
    //  - Every call with a filename throws InvalidAssetFilenameException when the name cannot be stored.
    //  - uploadAsset() throws AssetTransferCancelledException when the hooks cancel the upload; the file is then not
    //    guaranteed to be stored.
    //  - Network and protocol failures (including a key without write permission) are reported
    //    as CloudStorageException.
    class IAssetStorage
    {
    public:
        virtual ~IAssetStorage() = default;

        [[nodiscard]] virtual std::vector<AssetInfo> listAssets() const = 0;

        [[nodiscard]] virtual std::optional<AssetBytes> downloadAsset(const std::string& filename) const = 0;

        [[nodiscard]] virtual AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content, const AssetUploadHooks& hooks) const = 0;

        [[nodiscard]] AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content) const
        {
            return uploadAsset(filename, mimeType, content, AssetUploadHooks{});
        }

        [[nodiscard]] virtual bool deleteAsset(const std::string& filename) const = 0;

    protected:
        IAssetStorage() = default;
        IAssetStorage(const IAssetStorage&) = default;
        IAssetStorage& operator=(const IAssetStorage&) = default;
    };
}
