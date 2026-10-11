#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::update
{
    // Semantic Versioning 2.0.0. Build metadata is dropped: it takes no part in precedence.
    struct Semver
    {
        std::uint32_t major = 0;
        std::uint32_t minor = 0;
        std::uint32_t patch = 0;
        std::vector<std::string> prerelease;

        [[nodiscard]] std::string toString() const;

        friend bool operator==(const Semver&, const Semver&) = default;
        friend std::strong_ordering operator<=>(const Semver& left, const Semver& right);
    };

    // Accepts an optional leading 'v', as used in git tags ("v1.2.3").
    [[nodiscard]] std::optional<Semver> parseSemver(std::string_view text);
}
