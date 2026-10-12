#pragma once

#include <cstdint>

namespace stapik::cloud
{
    enum class AssetSizeStatus
    {
        Within,
        NearLimit,
        OverLimit
    };

    // maxBytes <= 0 means the limit is unknown, so no file is ever reported as near it.
    struct AssetSizeLimit
    {
        static constexpr int DEFAULT_WARN_PERCENT = 90;

        std::int64_t maxBytes{};
        int warnPercent = DEFAULT_WARN_PERCENT;

        [[nodiscard]] std::int64_t warnThresholdBytes() const
        {
            return maxBytes / 100 * warnPercent;
        }

        [[nodiscard]] AssetSizeStatus assess(const std::int64_t sizeBytes) const
        {
            using enum AssetSizeStatus;
            if (maxBytes <= 0)
                return Within;

            if (sizeBytes > maxBytes)
                return OverLimit;

            if (sizeBytes >= warnThresholdBytes())
                return NearLimit;

            return Within;
        }
    };
}
