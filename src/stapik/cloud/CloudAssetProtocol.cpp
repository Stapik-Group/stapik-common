#include "CloudAssetProtocol.hpp"
#include "CloudPartitionProtocol.hpp"

#include "stapik/sync/Timestamp.hpp"

#include <nlohmann/json.hpp>

#include <glib.h>

namespace stapik::cloud
{
    namespace
    {
        AssetInfo toAssetInfo(const nlohmann::json& item)
        {
            const auto updatedAtText = item.at("updatedAt").get<std::string>();
            const auto updatedAt = sync::parseIso8601(updatedAtText);
            if (!updatedAt)
                throw CloudStorageException("Invalid ISO-8601 timestamp: " + updatedAtText);

            return AssetInfo{
                .filename = item.at("filename").get<std::string>(),
                .mimeType = item.at("mimeType").get<std::string>(),
                .sizeBytes = item.at("sizeBytes").get<std::int64_t>(),
                .checksumSha256 = item.at("checksumSha256").get<std::string>(),
                .updatedAt = *updatedAt };
        }
    }

    std::string assetsUrl(const std::string& apiUrl, const std::string& slotKey)
    {
        return apiUrl + "/api/v1/assets/" + encodePathSegment(slotKey);
    }

    std::string assetUrl(const std::string& apiUrl, const std::string& slotKey, const std::string& filename)
    {
        return assetsUrl(apiUrl, slotKey) + "/" + encodePathSegment(filename);
    }

    std::vector<AssetInfo> parseAssetList(const std::string& body)
    {
        try
        {
            const auto json = nlohmann::json::parse(body);

            std::vector<AssetInfo> result;
            if (!json.contains("assets"))
                return result;

            for (const auto& item : json.at("assets"))
                result.push_back(toAssetInfo(item));

            return result;
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
        }
    }

    AssetInfo parseAssetMetadata(const std::string& body)
    {
        try
        {
            return toAssetInfo(nlohmann::json::parse(body));
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
        }
    }

    std::string sha256Hex(const AssetBytes& bytes)
    {
        // GLib refuses a null pointer even for an empty input.
        static constexpr guchar EMPTY_INPUT = 0;
        const guchar* data = bytes.empty() ? &EMPTY_INPUT : bytes.data();

        char* checksum = g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, bytes.size());
        std::string result(checksum);
        g_free(checksum);
        return result;
    }
}
