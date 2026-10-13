#pragma once

#include "stapik/http/HttpMessage.hpp"

#include <chrono>
#include <optional>

namespace stapik::cloud
{
    // The state of the per-minute request allowance of the API key, which the cloud adds to every authenticated
    // answer as X-RateLimit-Limit, X-RateLimit-Remaining and X-RateLimit-Reset (seconds until the full allowance
    // is restored).
    struct RateLimitState
    {
        int limit{};
        int remaining{};
        std::chrono::seconds resetAfter{};

        [[nodiscard]] bool operator==(const RateLimitState&) const = default;
    };

    // Empty unless all three headers are present and are non-negative whole numbers.
    [[nodiscard]] std::optional<RateLimitState> parseRateLimit(const http::Response& response);
}
