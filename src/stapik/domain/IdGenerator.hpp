#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace stapik::domain
{
    inline constexpr std::size_t GENERATED_ID_LENGTH = 16;
    inline constexpr std::size_t UUID_LENGTH = 36;

    // Random 64-bit identifier written as 16 lowercase hex digits. Thread safe.
    [[nodiscard]] std::string generateId();

    // Random UUID version 4 in the canonical 8-4-4-4-12 form. Thread safe.
    [[nodiscard]] std::string generateUuid();

    // Deterministic building blocks, exposed so the formats can be tested without randomness.
    [[nodiscard]] std::string formatHexId(std::uint64_t value);
    [[nodiscard]] std::string formatUuidV4(std::uint64_t highBits, std::uint64_t lowBits);
}
