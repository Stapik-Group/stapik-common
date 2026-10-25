#include "CompressedBlob.hpp"

#include "stapik/text/Base64.hpp"

namespace stapik::cloud
{
    bool isCompressedBlob(const nlohmann::json& content)
    {
        if (!content.is_object())
            return false;

        const auto format = content.find("format");
        return format != content.end() && format->is_string() && format->get<std::string>() == COMPRESSED_BLOB_FORMAT;
    }

    nlohmann::json compressedBlobOf(const std::string_view gzipStream)
    {
        auto blob = nlohmann::json::object();
        blob["format"] = COMPRESSED_BLOB_FORMAT;
        blob["version"] = COMPRESSED_BLOB_VERSION;
        blob["data"] = text::base64Encode(gzipStream);
        return blob;
    }

    std::optional<std::string> gzipStreamOf(const nlohmann::json& blob)
    {
        if (!isCompressedBlob(blob))
            return std::nullopt;

        const auto version = blob.find("version");
        if (version == blob.end() || !version->is_number_integer() || version->get<int>() != COMPRESSED_BLOB_VERSION)
            return std::nullopt;

        const auto data = blob.find("data");
        if (data == blob.end() || !data->is_string())
            return std::nullopt;

        return text::base64Decode(data->get<std::string>());
    }
}
