#pragma once

#include "IHttpClient.hpp"

namespace stapik::http
{
    class CurlHttpClient final : public IHttpClient
    {
    public:
        [[nodiscard]] SendResult send(const Request& request) const override;
    };
}
