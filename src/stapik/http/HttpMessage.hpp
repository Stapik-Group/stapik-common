#pragma once

#include "stapik/core/Result.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace stapik::http
{
    using Bytes = std::vector<std::uint8_t>;

    enum class Method
    {
        Get,
        Put,
        Delete
    };

    struct NoBody
    {};

    struct MultipartFile
    {
        std::string fieldName;
        std::string filename;
        std::string mimeType;
        Bytes content;
    };

    using RequestBody = std::variant<NoBody, Bytes, MultipartFile>;

    inline constexpr std::chrono::seconds NO_TIMEOUT{ 0 };

    struct TransferProgress
    {
        std::uint64_t uploadedBytes{};
        std::uint64_t uploadTotalBytes{};
        std::uint64_t downloadedBytes{};
        std::uint64_t downloadTotalBytes{};
    };

    using ProgressCallback = std::function<void(const TransferProgress&)>;
    using CancelCheck = std::function<bool()>;

    struct Request
    {
        Method method = Method::Get;
        std::string url;
        std::map<std::string, std::string> headers;
        RequestBody body;
        std::chrono::seconds timeout{ 30 };
        std::chrono::seconds connectTimeout{ 10 };
        std::chrono::seconds stallTimeout{ 30 };
        std::optional<std::size_t> maxResponseBytes;
        ProgressCallback onProgress;
        CancelCheck isCancelled;
    };

    struct Response
    {
        int status{};
        Bytes body;
        std::optional<std::chrono::seconds> retryAfter;
        // Headers of the final response, names in lower case. A repeated header keeps its last value.
        std::map<std::string, std::string> headers;

        [[nodiscard]] std::optional<std::string> header(const std::string_view lowerCaseName) const
        {
            if (const auto found = headers.find(std::string(lowerCaseName)); found != headers.end())
                return found->second;

            return std::nullopt;
        }
    };

    struct TransportError
    {
        std::string message;
        bool cancelled = false;
    };

    using SendResult = Result<Response, TransportError>;

    [[nodiscard]] Bytes toBytes(std::string_view text);
    [[nodiscard]] std::string toText(const Bytes& bytes);
}
