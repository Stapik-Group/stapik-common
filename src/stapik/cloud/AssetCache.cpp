#include "AssetCache.hpp"
#include "CloudAssetProtocol.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AtomicFile.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>

namespace stapik::cloud
{
    namespace
    {
        constexpr std::size_t SHA256_HEX_LENGTH = 64;

        // Lowercase checksum usable as a file name, or an empty string when the value is not a SHA-256 digest.
        std::string normalizedChecksum(std::string checksum)
        {
            const bool isHexDigest = checksum.size() == SHA256_HEX_LENGTH &&
                std::ranges::all_of(checksum, [](const unsigned char character) { return std::isxdigit(character) != 0; });
            if (!isHexDigest)
                return {};

            std::ranges::transform(checksum, checksum.begin(), [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return checksum;
        }

        std::optional<AssetBytes> readFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return std::nullopt;

            return AssetBytes(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
        }
    }

    AssetCache::AssetCache(std::filesystem::path directory) :
        m_directory(std::move(directory))
    {}

    std::filesystem::path AssetCache::entryPath(const std::string& checksum) const
    {
        return m_directory / checksum;
    }

    std::optional<AssetBytes> AssetCache::find(const AssetInfo& info) const
    {
        const auto checksum = normalizedChecksum(info.checksumSha256);
        if (checksum.empty())
            return std::nullopt;

        const auto path = entryPath(checksum);
        auto content = readFile(path);
        if (!content)
            return std::nullopt;

        if (sha256Hex(*content) != checksum)
        {
            log::warning("Asset cache entry {} does not match its checksum, discarding it", checksum);
            std::error_code errorCode;
            std::filesystem::remove(path, errorCode);
            return std::nullopt;
        }

        return content;
    }

    AssetCacheResult AssetCache::store(const AssetInfo& info, const AssetBytes& content) const
    {
        const auto checksum = normalizedChecksum(info.checksumSha256);
        if (checksum.empty())
            return Failure(AssetCacheError{ .reason = AssetCacheError::Reason::InvalidChecksum, .message = std::format("Asset '{}' has no valid SHA-256 checksum", info.filename) });

        if (sha256Hex(content) != checksum)
            return Failure(AssetCacheError{ .reason = AssetCacheError::Reason::ChecksumMismatch, .message = std::format("Content of asset '{}' does not match its checksum", info.filename) });

        const std::string_view bytes(reinterpret_cast<const char*>(content.data()), content.size());
        if (const auto written = storage::writeFileAtomically(entryPath(checksum), bytes); !written)
            return Failure(AssetCacheError{ .reason = AssetCacheError::Reason::WriteFailed, .message = written.error().message() });

        return {};
    }

    std::optional<AssetBytes> AssetCache::fetch(const IAssetStorage& storage, const AssetInfo& info) const
    {
        if (auto cached = find(info))
            return cached;

        auto downloaded = storage.downloadAsset(info.filename);
        if (!downloaded)
            return std::nullopt;

        const auto checksum = normalizedChecksum(info.checksumSha256);
        if (checksum.empty())
            return downloaded;

        if (sha256Hex(*downloaded) != checksum)
            throw CloudStorageException(std::format("Downloaded file '{}' does not match its checksum", info.filename));

        if (const auto stored = store(info, *downloaded); !stored)
            log::warning("Asset '{}' was not cached: {}", info.filename, stored.error().message);
        return downloaded;
    }

    void AssetCache::retainOnly(const std::vector<AssetInfo>& assets) const
    {
        std::set<std::string> kept;
        for (const auto& asset : assets)
            kept.insert(normalizedChecksum(asset.checksumSha256));

        std::error_code errorCode;
        std::filesystem::directory_iterator entries(m_directory, errorCode);
        if (errorCode)
            return;

        for (const auto& entry : entries)
        {
            if (!kept.contains(entry.path().filename().string()))
                std::filesystem::remove(entry.path(), errorCode);
        }
    }
}
