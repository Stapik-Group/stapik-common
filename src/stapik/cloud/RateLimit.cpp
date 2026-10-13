#include "RateLimit.hpp"

#include <charconv>
#include <string>

namespace stapik::cloud
{
    namespace
    {
        std::optional<int> headerNumber(const http::Response& response, const std::string_view name)
        {
            const auto text = response.header(name);
            if (!text || text->empty())
                return std::nullopt;

            int value = 0;
            const auto* const end = text->data() + text->size();
            const auto [stopped, error] = std::from_chars(text->data(), end, value);
            if (error != std::errc{} || stopped != end || value < 0)
                return std::nullopt;

            return value;
        }
    }

    std::optional<RateLimitState> parseRateLimit(const http::Response& response)
    {
        const auto limit = headerNumber(response, "x-ratelimit-limit");
        const auto remaining = headerNumber(response, "x-ratelimit-remaining");
        const auto reset = headerNumber(response, "x-ratelimit-reset");
        if (!limit || !remaining || !reset)
            return std::nullopt;

        return RateLimitState{ .limit = *limit, .remaining = *remaining, .resetAfter = std::chrono::seconds{ *reset } };
    }
}
