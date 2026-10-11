#include "Semver.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <format>
#include <tuple>

namespace stapik::update
{
    namespace
    {
        bool isDigits(const std::string_view text)
        {
            return !text.empty() && std::ranges::all_of(text, [](const char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
        }

        bool isIdentifier(const std::string_view text)
        {
            return !text.empty() && std::ranges::all_of(text, [](const char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-'; });
        }

        bool hasLeadingZero(const std::string_view digits)
        {
            return digits.size() > 1 && digits.front() == '0';
        }

        std::vector<std::string_view> split(const std::string_view text, const char separator)
        {
            std::vector<std::string_view> parts;
            std::size_t start = 0;
            while (true)
            {
                const auto end = text.find(separator, start);
                parts.push_back(text.substr(start, end == std::string_view::npos ? end : end - start));
                if (end == std::string_view::npos)
                    return parts;

                start = end + 1;
            }
        }

        std::optional<std::uint32_t> parseNumber(const std::string_view text)
        {
            if (!isDigits(text) || hasLeadingZero(text))
                return std::nullopt;

            std::uint32_t value = 0;
            if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value); error != std::errc{})
                return std::nullopt;

            return value;
        }

        bool isValidPrereleaseIdentifier(const std::string_view text)
        {
            return isIdentifier(text) && !(isDigits(text) && hasLeadingZero(text));
        }

        // Numeric identifiers have no leading zeros, so a longer one is always the greater one.
        std::strong_ordering compareIdentifiers(const std::string& left, const std::string& right)
        {
            const bool leftNumeric = isDigits(left);
            const bool rightNumeric = isDigits(right);

            if (leftNumeric && rightNumeric)
            {
                if (left.size() != right.size())
                    return left.size() <=> right.size();
                return left.compare(right) <=> 0;
            }

            if (leftNumeric != rightNumeric)
                return leftNumeric ? std::strong_ordering::less : std::strong_ordering::greater;

            return left.compare(right) <=> 0;
        }
    }

    std::string Semver::toString() const
    {
        std::string text = std::format("{}.{}.{}", std::to_string(major), std::to_string(minor), std::to_string(patch));
        for (std::size_t index = 0; index < prerelease.size(); ++index)
            text += (index == 0 ? '-' : '.') + prerelease[index];

        return text;
    }

    std::strong_ordering operator<=>(const Semver& left, const Semver& right)
    {
        if (const auto core = std::tie(left.major, left.minor, left.patch) <=> std::tie(right.major, right.minor, right.patch); core != 0)
            return core;

        // A version without a prerelease part is greater than the same version with one.
        if (left.prerelease.empty() || right.prerelease.empty())
            return left.prerelease.empty() <=> right.prerelease.empty();

        const auto common = std::min(left.prerelease.size(), right.prerelease.size());
        for (std::size_t index = 0; index < common; ++index)
        {
            if (const auto order = compareIdentifiers(left.prerelease[index], right.prerelease[index]); order != 0)
                return order;
        }

        return left.prerelease.size() <=> right.prerelease.size();
    }

    std::optional<Semver> parseSemver(std::string_view text)
    {
        if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
            text.remove_prefix(1);

        if (const auto metadata = text.find('+'); metadata != std::string_view::npos)
        {
            if (const auto identifiers = split(text.substr(metadata + 1), '.'); !std::ranges::all_of(identifiers, isIdentifier))
                return std::nullopt;

            text = text.substr(0, metadata);
        }

        Semver version;
        if (const auto dash = text.find('-'); dash != std::string_view::npos)
        {
            for (const auto identifier : split(text.substr(dash + 1), '.'))
            {
                if (!isValidPrereleaseIdentifier(identifier))
                    return std::nullopt;

                version.prerelease.emplace_back(identifier);
            }

            text = text.substr(0, dash);
        }

        const auto core = split(text, '.');
        if (core.size() != 3)
            return std::nullopt;

        const auto major = parseNumber(core[0]);
        const auto minor = parseNumber(core[1]);
        const auto patch = parseNumber(core[2]);
        if (!major || !minor || !patch)
            return std::nullopt;

        version.major = *major;
        version.minor = *minor;
        version.patch = *patch;
        return version;
    }
}
