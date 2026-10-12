#pragma once

#include "stapik/core/Result.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace stapik::compression
{
    enum class GzipError
    {
        CompressionFailed,
        Malformed,
        TooLarge
    };

    using GzipResult = Result<std::string, GzipError>;

    [[nodiscard]] std::string_view describe(GzipError error);

    [[nodiscard]] GzipResult gzipCompress(std::string_view content);
    [[nodiscard]] GzipResult gzipDecompress(std::string_view compressed, std::size_t maxDecompressedSize);
}
