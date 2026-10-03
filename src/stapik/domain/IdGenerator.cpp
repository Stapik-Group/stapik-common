#include "IdGenerator.hpp"

#include <array>
#include <random>
#include <string_view>

namespace stapik::domain
{
    namespace
    {
        constexpr std::string_view HEX_DIGITS = "0123456789abcdef";
        constexpr std::uint64_t UUID_VERSION_MASK = 0xFFFFFFFFFFFF0FFFULL;
        constexpr std::uint64_t UUID_VERSION_4 = 0x0000000000004000ULL;
        constexpr std::uint64_t UUID_VARIANT_MASK = 0x3FFFFFFFFFFFFFFFULL;
        constexpr std::uint64_t UUID_VARIANT_RFC4122 = 0x8000000000000000ULL;

        std::mt19937_64& threadLocalEngine()
        {
            thread_local std::mt19937_64 engine = []
            {
                std::random_device randomDevice;
                std::array<std::uint32_t, 8> seedValues{};
                for (auto& seedValue : seedValues)
                    seedValue = randomDevice();

                std::seed_seq seedSequence(seedValues.begin(), seedValues.end());
                return std::mt19937_64(seedSequence);
            }();

            return engine;
        }

        void appendHex(std::string& target, const std::uint64_t value, const int digitCount)
        {
            for (int digitIndex = digitCount - 1; digitIndex >= 0; --digitIndex)
            {
                const auto nibble = (value >> digitIndex * 4) & 0xF;
                target.push_back(HEX_DIGITS[nibble]);
            }
        }
    }

    std::string generateId()
    {
        return formatHexId(threadLocalEngine()());
    }

    std::string generateUuid()
    {
        auto& engine = threadLocalEngine();
        const auto highBits = engine();
        const auto lowBits = engine();
        return formatUuidV4(highBits, lowBits);
    }

    std::string formatHexId(const std::uint64_t value)
    {
        std::string result;
        result.reserve(GENERATED_ID_LENGTH);
        appendHex(result, value, GENERATED_ID_LENGTH);
        return result;
    }

    std::string formatUuidV4(const std::uint64_t highBits, const std::uint64_t lowBits)
    {
        const auto versionedHighBits = (highBits & UUID_VERSION_MASK) | UUID_VERSION_4;
        const auto variantLowBits = (lowBits & UUID_VARIANT_MASK) | UUID_VARIANT_RFC4122;

        std::string result;
        result.reserve(UUID_LENGTH);
        appendHex(result, versionedHighBits >> 32, 8);
        result.push_back('-');
        appendHex(result, versionedHighBits >> 16, 4);
        result.push_back('-');
        appendHex(result, versionedHighBits, 4);
        result.push_back('-');
        appendHex(result, variantLowBits >> 48, 4);
        result.push_back('-');
        appendHex(result, variantLowBits, 12);
        return result;
    }
}
