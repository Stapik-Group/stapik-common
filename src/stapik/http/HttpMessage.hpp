#pragma once

#include "stapik/core/Result.hpp"

#include <chrono>
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
    };

    struct Response
    {
        int status{};
        Bytes body;
    };

    struct TransportError
    {
        std::string message;
    };

    using SendResult = Result<Response, TransportError>;

    [[nodiscard]] Bytes toBytes(std::string_view text);
    [[nodiscard]] std::string toText(const Bytes& bytes);
}
