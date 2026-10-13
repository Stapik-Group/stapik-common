#include "ApiKeyExpiry.hpp"

#include "stapik/sync/Timestamp.hpp"

namespace stapik::cloud
{
    std::optional<std::chrono::system_clock::time_point> parseApiKeyExpiry(const http::Response& response)
    {
        const auto header = response.header("x-api-key-expires-at");
        if (!header)
            return std::nullopt;

        return sync::parseIso8601(*header);
    }

    void ApiKeyExpiryMonitor::observe(const http::Response& response)
    {
        const auto expiresAt = parseApiKeyExpiry(response);

        const std::scoped_lock lock(m_mutex);
        m_expiresAt = expiresAt;
    }

    void ApiKeyExpiryMonitor::reset()
    {
        const std::scoped_lock lock(m_mutex);
        m_expiresAt.reset();
    }

    auto ApiKeyExpiryMonitor::expiresAt() const -> std::optional<TimePoint>
    {
        const std::scoped_lock lock(m_mutex);
        return m_expiresAt;
    }
}
