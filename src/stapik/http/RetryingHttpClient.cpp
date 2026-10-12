#include "RetryingHttpClient.hpp"

#include "stapik/log/Log.hpp"

#include <algorithm>
#include <thread>
#include <utility>

namespace stapik::http
{
    namespace
    {
        constexpr int HTTP_TOO_MANY_REQUESTS = 429;
        constexpr int HTTP_SERVICE_UNAVAILABLE = 503;
        constexpr std::chrono::milliseconds CANCEL_POLL_INTERVAL{ 100 };

        bool isCancelledNow(const CancelCheck& isCancelled)
        {
            return isCancelled && isCancelled();
        }
    }

    RetryingHttpClient::RetryingHttpClient(const IHttpClient& inner, const RetryPolicy policy, Sleep sleep) :
        m_inner(inner),
        m_policy(policy),
        m_sleep(sleep ? std::move(sleep) : Sleep([](const std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); }))
    {
    }

    SendResult RetryingHttpClient::send(const Request& request) const
    {
        for (int retriesDone = 0;; ++retriesDone)
        {
            auto result = m_inner.send(request);
            if (!result || !shouldRetry(result.value(), retriesDone))
                return result;

            const auto delay = *result.value().retryAfter;
            log::debug("Server answered {} and asked to wait {} s, retrying", result.value().status, delay.count());

            if (!waitFor(delay, request.isCancelled))
                return Failure(TransportError{ .message = "Transfer cancelled", .cancelled = true });
        }
    }

    bool RetryingHttpClient::shouldRetry(const Response& response, const int retriesDone) const
    {
        const bool retryableStatus = response.status == HTTP_TOO_MANY_REQUESTS || response.status == HTTP_SERVICE_UNAVAILABLE;

        return retryableStatus
               && retriesDone < m_policy.maxRetries
               && response.retryAfter.has_value()
               && *response.retryAfter <= m_policy.maxDelay;
    }

    bool RetryingHttpClient::waitFor(const std::chrono::seconds delay, const CancelCheck& isCancelled) const
    {
        for (auto left = std::chrono::duration_cast<std::chrono::milliseconds>(delay); left > std::chrono::milliseconds::zero();)
        {
            if (isCancelledNow(isCancelled))
                return false;

            const auto step = std::min(left, CANCEL_POLL_INTERVAL);
            m_sleep(step);
            left -= step;
        }

        return !isCancelledNow(isCancelled);
    }
}
