#pragma once

#include "IHttpClient.hpp"

#include <chrono>
#include <functional>

namespace stapik::http
{
    struct RetryPolicy
    {
        int maxRetries = 2;
        std::chrono::seconds maxDelay{ 30 };
    };

    class RetryingHttpClient final : public IHttpClient
    {
    public:
        using Sleep = std::function<void(std::chrono::milliseconds)>;

        explicit RetryingHttpClient(const IHttpClient& inner, RetryPolicy policy = {}, Sleep sleep = {});

        [[nodiscard]] SendResult send(const Request& request) const override;

    private:
        [[nodiscard]] bool shouldRetry(const Request& request, const Response& response, int retriesDone) const;
        [[nodiscard]] bool waitFor(std::chrono::seconds delay, const CancelCheck& isCancelled) const;

        const IHttpClient& m_inner;
        RetryPolicy m_policy;
        Sleep m_sleep;
    };
}
