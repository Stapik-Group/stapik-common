#pragma once

#include "stapik/http/HttpMessage.hpp"

#include <chrono>
#include <mutex>
#include <optional>

namespace stapik::cloud
{
    // The cloud adds X-Api-Key-Expires-At (ISO-8601) to its answers once the API key is about to expire
    // (by default 30 days before). A missing or unreadable header means there is nothing to warn about.
    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> parseApiKeyExpiry(const http::Response& response);

    // Remembers what the last answer of the cloud said about the expiry of the API key. It is filled by the
    // clients, which work on background threads, and read by whoever shows the warning, so it is thread safe.
    class ApiKeyExpiryMonitor
    {
    public:
        using TimePoint = std::chrono::system_clock::time_point;

        void observe(const http::Response& response);
        void reset();

        [[nodiscard]] std::optional<TimePoint> expiresAt() const;

    private:
        mutable std::mutex m_mutex;
        std::optional<TimePoint> m_expiresAt;
    };
}
