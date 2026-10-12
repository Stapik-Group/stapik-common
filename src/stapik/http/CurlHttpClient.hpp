#pragma once

#include "IHttpClient.hpp"

#include <functional>
#include <string>

namespace stapik::http
{
    class CurlHttpClient final : public IHttpClient
    {
    public:
        using UserAgentProvider = std::function<std::string()>;
        explicit CurlHttpClient(UserAgentProvider userAgent = {});
        [[nodiscard]] SendResult send(const Request& request) const override;
    private:
        UserAgentProvider m_userAgent;
    };
}
