#include "CloudPartitionProtocol.hpp"
#include "CloudStorageException.hpp"

#include "stapik/sync/Timestamp.hpp"

#include <nlohmann/json.hpp>

namespace stapik::cloud
{
    namespace
    {
        bool isUnreserved(const char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '-' || c == '.' || c == '_' || c == '~';
        }
    }

    std::string encodePathSegment(const std::string& segment)
    {
        std::string encoded;

        for (const char c : segment)
        {
            constexpr char hex[] = "0123456789ABCDEF";
            if (isUnreserved(c))
            {
                encoded += c;
                continue;
            }

            const auto byte = static_cast<std::byte>(static_cast<unsigned char>(c));
            encoded += '%';
            encoded += hex[std::to_integer<unsigned int>(byte >> 4)];
            encoded += hex[std::to_integer<unsigned int>(byte & std::byte{0x0F})];
        }

        return encoded;
    }

    std::string partitionsUrl(const std::string& documentUrl)
    {
        return documentUrl + "/partitions";
    }

    std::string partitionUrl(const std::string& documentUrl, const std::string& partition)
    {
        return partitionsUrl(documentUrl) + "/" + encodePathSegment(partition);
    }

    std::vector<CloudPartitionInfo> parsePartitionList(const std::string& body)
    {
        try
        {
            const auto json = nlohmann::json::parse(body);

            std::vector<CloudPartitionInfo> result;
            for (const auto& item : json.at("partitions"))
            {
                const auto updatedAtText = item.at("updatedAt").get<std::string>();
                const auto updatedAt = sync::parseIso8601(updatedAtText);
                if (!updatedAt)
                    throw CloudStorageException("Invalid ISO-8601 timestamp: " + updatedAtText);

                result.push_back(CloudPartitionInfo{
                    .partition = item.at("partition").get<std::string>(),
                    .sizeBytes = item.at("sizeBytes").get<std::int64_t>(),
                    .contentHash = item.at("contentHash").get<std::string>(),
                    .updatedAt = *updatedAt });
            }
            return result;
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
        }
    }
}
