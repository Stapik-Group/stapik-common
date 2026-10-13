#pragma once

#include "HttpMessage.hpp"

namespace stapik::http
{
    // Blocking transport of a single HTTP request. Any response, whatever its status, is a success; only a request
    // that got no response at all (connection, TLS, timeout, an oversized body) is a TransportError.
    class IHttpClient
    {
    public:
        virtual ~IHttpClient() = default;

        [[nodiscard]] virtual SendResult send(const Request& request) const = 0;

    protected:
        IHttpClient() = default;
        IHttpClient(const IHttpClient&) = default;
        IHttpClient& operator=(const IHttpClient&) = default;
    };

    // The client used wherever a caller does not pass one: libcurl restricted to http(s) and TLS 1.2+, which waits and
    // tries again when the server answers 429 or 503 with a short Retry-After (a POST or PATCH only after a 429).
    [[nodiscard]] const IHttpClient& defaultHttpClient();
}
