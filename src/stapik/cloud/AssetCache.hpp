#pragma once

#include "IAssetStorage.hpp"

#include "stapik/core/Result.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace stapik::cloud
{
    struct AssetCacheError
    {
        enum class Reason
        {
            InvalidChecksum,
            ChecksumMismatch,
            WriteFailed
        };

        Reason reason;
        std::string message;
    };

    using AssetCacheResult = Result<void, AssetCacheError>;

    class AssetCache
    {
    public:
        explicit AssetCache(std::filesystem::path directory);

        [[nodiscard]] std::optional<AssetBytes> find(const AssetInfo& info) const;

        // Keeps nothing when the content does not match info.checksumSha256 or cannot be written.
        [[nodiscard]] AssetCacheResult store(const AssetInfo& info, const AssetBytes& content) const;

        [[nodiscard]] std::optional<AssetBytes> fetch(const IAssetStorage& storage, const AssetInfo& info) const;

        void retainOnly(const std::vector<AssetInfo>& assets) const;

    private:
        [[nodiscard]] std::filesystem::path entryPath(const std::string& checksum) const;

        std::filesystem::path m_directory;
    };
}
