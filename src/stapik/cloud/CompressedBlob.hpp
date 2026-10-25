#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace stapik::cloud
{
    inline constexpr std::string_view COMPRESSED_BLOB_FORMAT = "stapik-gzip";
    inline constexpr int COMPRESSED_BLOB_VERSION = 1;
    inline constexpr std::size_t COMPRESSED_BLOB_MEMBERS = 3;

    [[nodiscard]] bool isCompressedBlob(const nlohmann::json& content);
    [[nodiscard]] nlohmann::json compressedBlobOf(std::string_view gzipStream);
    [[nodiscard]] std::optional<std::string> gzipStreamOf(const nlohmann::json& blob);
}
