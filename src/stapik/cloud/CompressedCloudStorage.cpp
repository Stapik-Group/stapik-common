#include "CompressedCloudStorage.hpp"

#include "CloudStorageException.hpp"

#include "stapik/compression/Gzip.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/text/Base64.hpp"

#include <format>
#include <utility>

namespace stapik::cloud
{
    namespace
    {
        constexpr auto FORMAT = "stapik-gzip";
        constexpr int VERSION = 1;

        nlohmann::json blobOf(const std::string& encodedData)
        {
            auto blob = nlohmann::json::object();
            blob["format"] = FORMAT;
            blob["version"] = VERSION;
            blob["data"] = encodedData;
            return blob;
        }
    }

    CompressedCloudStorage::CompressedCloudStorage(std::unique_ptr<ICloudStorage> inner, const std::size_t thresholdBytes) :
        TransformingCloudStorage(std::move(inner)),
        m_thresholdBytes(thresholdBytes)
    {}

    bool CompressedCloudStorage::isCompressed(const nlohmann::json& content)
    {
        if (!content.is_object())
            return false;

        const auto format = content.find("format");
        return format != content.end() && format->is_string() && format->get<std::string>() == FORMAT;
    }

    nlohmann::json CompressedCloudStorage::seal(const nlohmann::json& data, const std::string&) const
    {
        const auto text = data.dump();
        if (text.size() < m_thresholdBytes)
            return data;

        const auto compressed = compression::gzipCompress(text);
        if (!compressed)
        {
            log::warning("Cannot compress the cloud data, sending it as is: {}", compression::describe(compressed.error()));
            return data;
        }

        const auto encoded = text::base64Encode(compressed.value());
        if (encoded.size() >= text.size())
            return data;

        return blobOf(encoded);
    }

    CloudDocument CompressedCloudStorage::open(CloudDocument document, const std::string&) const
    {
        if (!isCompressed(document.content))
            return document;

        if (const auto version = document.content.find("version"); version == document.content.end() || !version->is_number_integer() || version->get<int>() != VERSION)
            throw CloudStorageException("The cloud data was compressed by a different version of the application");

        const auto data = document.content.find("data");
        if (data == document.content.end() || !data->is_string())
            throw CloudStorageException("The compressed cloud data is damaged");

        const auto compressed = text::base64Decode(data->get<std::string>());
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
