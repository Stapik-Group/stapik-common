#pragma once

#include <algorithm>
#include <chrono>

namespace stapik::sync
{
    [[nodiscard]] inline std::chrono::milliseconds backoffDelay(
        const std::chrono::milliseconds initialDelay,
        const std::chrono::milliseconds maxDelay,
        const int consecutiveFailures)
    {
        auto delay = initialDelay;

        for (int failure = 1; failure < consecutiveFailures && delay < maxDelay; ++failure)
            delay *= 2;

        return std::min(delay, maxDelay);
    }
}
