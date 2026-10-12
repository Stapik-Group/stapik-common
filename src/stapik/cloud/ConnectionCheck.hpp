#pragma once

#include "CloudStorageConfig.hpp"

#include "stapik/core/Result.hpp"
#include "stapik/http/IHttpClient.hpp"

#include <string>

namespace stapik::cloud
{
    enum class ConnectionErrorKind
    {
        Unreachable,
        InvalidApiKey,
        UnexpectedStatus
    };

    struct ConnectionError
    {
        ConnectionErrorKind kind{};
        int httpStatus{};
        std::string detail;
    };

    using ConnectionCheckResult = Result<void, ConnectionError>;

    [[nodiscard]] ConnectionCheckResult checkConnection(
        const CloudStorageConfig& config,
        const http::IHttpClient& httpClient = http::defaultHttpClient());
}
