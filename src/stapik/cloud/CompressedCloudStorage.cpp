#include "CompressedCloudStorage.hpp"

#include "CloudStorageException.hpp"
#include "CompressedBlob.hpp"

#include "stapik/compression/Gzip.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/text/JsonText.hpp"

#include <format>
#include <utility>

namespace stapik::cloud
{
    CompressedCloudStorage::CompressedCloudStorage(std::unique_ptr<ICloudStorage> inner, const std::size_t thresholdBytes) :
        TransformingCloudStorage(std::move(inner)),
        m_thresholdBytes(thresholdBytes)
    {}

    bool CompressedCloudStorage::isCompressed(const nlohmann::json& content)
    {
        return isCompressedBlob(content);
    }

    nlohmann::json CompressedCloudStorage::seal(const nlohmann::json& data, const std::string&) const
    {
        const auto serialized = text::dumpJson(data);
        if (serialized.size() < m_thresholdBytes)
            return data;

        const auto compressed = compression::gzipCompress(serialized);
        if (!compressed)
        {
            log::warning("Cannot compress the cloud data, sending it as is: {}", compression::describe(compressed.error()));
            return data;
        }

        auto blob = compressedBlobOf(compressed.value());
        if (blob.at("data").get_ref<const std::string&>().size() >= serialized.size())
            return data;

        return blob;
    }

    CloudDocument CompressedCloudStorage::open(CloudDocument document, const std::string&) const
    {
        if (!isCompressed(document.content))
            return document;

        if (const auto version = document.content.find("version"); version == document.content.end() || !version->is_number_integer() || version->get<int>() != COMPRESSED_BLOB_VERSION)
            throw CloudStorageException("The cloud data was compressed by a different version of the application");

        const auto compressed = gzipStreamOf(document.content);
        if (!compressed)
            throw CloudStorageException("The compressed cloud data is damaged");

        const auto text = compression::gzipDecompress(*compressed, MAX_DECOMPRESSED_BYTES);
        if (!text)
            throw CloudStorageException(std::format("Cannot decompress the cloud data: {}", compression::describe(text.error())));

        try
        {
            document.content = nlohmann::json::parse(text.value());
        }
        catch (const nlohmann::json::exception&)
        {
            throw CloudStorageException("Cannot read the decompressed cloud data");
        }

        return document;
    }
}
