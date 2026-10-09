#pragma once

#include "CloudStorageException.hpp"

#include <chrono>
#include <cstdint>
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

    // The server refuses a file bigger than the maximum file size of the slot (HTTP 413).
    // Uploading the same file again can never succeed, so callers should not retry it.
    class AssetTooLargeException : public ::CloudStorageException
    {
    public:
        using ::CloudStorageException::CloudStorageException;
    };

    // Abstraction of a remote collection of binary files (a BINARY_COLLECTION slot). It allows the
    // logic built on top of it to be tested against a fake without any network access.
    //
    // Contract:
    //  - A file is identified by its filename; uploading under an existing filename overwrites it.
    //  - listAssets() returns an empty list when the slot does not exist or the storage is not
    //    configured; downloadAsset() returns std::nullopt when the file does not exist or the storage
    //    is not configured; deleteAsset() returns false when there was nothing to delete.
    //  - uploadAsset() throws AssetTooLargeException when the file exceeds the size limit of the slot.
    //  - Network and protocol failures (including a key without write permission) are reported
    //    as CloudStorageException.
    class IAssetStorage
    {
    public:
        virtual ~IAssetStorage() = default;

        [[nodiscard]] virtual std::vector<AssetInfo> listAssets() const = 0;

        [[nodiscard]] virtual std::optional<AssetBytes> downloadAsset(const std::string& filename) const = 0;

        virtual AssetInfo uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content) const = 0;

        [[nodiscard]] virtual bool deleteAsset(const std::string& filename) const = 0;

    protected:
        IAssetStorage() = default;
        IAssetStorage(const IAssetStorage&) = default;
        IAssetStorage& operator=(const IAssetStorage&) = default;
    };
}
