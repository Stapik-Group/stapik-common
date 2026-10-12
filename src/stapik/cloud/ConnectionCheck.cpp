#include "ConnectionCheck.hpp"
#include "CloudAssetProtocol.hpp"

#include <chrono>

namespace stapik::cloud
{
    namespace
    {
        constexpr auto PROBE_SLOT_KEY = "connection-check";
        constexpr std::chrono::seconds TIMEOUT{ 8 };
        constexpr std::chrono::seconds CONNECT_TIMEOUT{ 5 };
        constexpr std::size_t MAX_RESPONSE_BYTES = 1024U * 1024U;
        constexpr int HTTP_OK = 200;
        constexpr int HTTP_UNAUTHORIZED = 401;
        constexpr int HTTP_FORBIDDEN = 403;
        constexpr int HTTP_NOT_FOUND = 404;

        Failure<ConnectionError> connectionFailure(const ConnectionErrorKind kind, const int httpStatus, std::string detail)
        {
            return Failure(ConnectionError{ .kind = kind, .httpStatus = httpStatus, .detail = std::move(detail) });
        }
    }

    ConnectionCheckResult checkConnection(const CloudStorageConfig& config, const http::IHttpClient& httpClient)
    {
        const http::Request request{
            .method = http::Method::Get,
            .url = assetsUrl(config.normalizedApiUrl(), PROBE_SLOT_KEY),
            .headers = { { "x-api-key", config.apiKey } },
            .timeout = TIMEOUT,
            .connectTimeout = CONNECT_TIMEOUT,
            .maxResponseBytes = MAX_RESPONSE_BYTES
        };

        const auto result = httpClient.send(request);
        if (!result)
            return connectionFailure(ConnectionErrorKind::Unreachable, 0, result.error().message);

        const auto status = result.value().status;
        if (status == HTTP_OK || status == HTTP_NOT_FOUND)
            return {};

        if (status == HTTP_UNAUTHORIZED || status == HTTP_FORBIDDEN)
            return connectionFailure(ConnectionErrorKind::InvalidApiKey, status, {});

        return connectionFailure(ConnectionErrorKind::UnexpectedStatus, status, {});
    }
}
