#include "Base64.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace stapik::text
{
    namespace
    {
        constexpr std::string_view ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        constexpr char PADDING = '=';
        constexpr int INVALID_SYMBOL = -1;

        constexpr std::array<int, 256> makeDecodeTable()
        {
            std::array<int, 256> table{};
            table.fill(INVALID_SYMBOL);
            for (std::size_t index = 0; index < ALPHABET.size(); ++index)
                table[static_cast<unsigned char>(ALPHABET[index])] = static_cast<int>(index);

            return table;
        }

        constexpr auto DECODE_TABLE = makeDecodeTable();

        std::uint32_t byteAt(const std::string_view bytes, const std::size_t index)
        {
            return static_cast<unsigned char>(bytes[index]);
        }
    }

    std::string base64Encode(const std::string_view bytes)
    {
        std::string encoded;
        encoded.reserve((bytes.size() + 2) / 3 * 4);

        for (std::size_t offset = 0; offset < bytes.size(); offset += 3)
        {
            const std::size_t available = std::min<std::size_t>(3, bytes.size() - offset);

            std::uint32_t chunk = byteAt(bytes, offset) << 16;
            if (available > 1)
                chunk |= byteAt(bytes, offset + 1) << 8;
            if (available > 2)
                chunk |= byteAt(bytes, offset + 2);

            encoded.push_back(ALPHABET[(chunk >> 18) & 0x3F]);
            encoded.push_back(ALPHABET[(chunk >> 12) & 0x3F]);
            encoded.push_back(available > 1 ? ALPHABET[(chunk >> 6) & 0x3F] : PADDING);
            encoded.push_back(available > 2 ? ALPHABET[chunk & 0x3F] : PADDING);
        }

        return encoded;
    }

    std::optional<std::string> base64Decode(const std::string_view text)
    {
        if (text.size() % 4 != 0)
            return std::nullopt;

        std::string decoded;
        decoded.reserve(text.size() / 4 * 3);

        for (std::size_t offset = 0; offset < text.size(); offset += 4)
        {
            const bool lastGroup = offset + 4 == text.size();
            int paddingCount = 0;
            std::uint32_t chunk = 0;

            for (std::size_t position = 0; position < 4; ++position)
            {
                const char symbol = text[offset + position];
                int value = 0;

                if (symbol == PADDING)
                {
                    if (!lastGroup || position < 2)
                        return std::nullopt;
                    ++paddingCount;
                }
                else
                {
                    value = DECODE_TABLE[static_cast<unsigned char>(symbol)];
                    if (paddingCount > 0 || value == INVALID_SYMBOL)
                        return std::nullopt;
                }

                chunk = (chunk << 6) | static_cast<std::uint32_t>(value);
            }

            decoded.push_back(static_cast<char>((chunk >> 16) & 0xFF));
            if (paddingCount < 2)
                decoded.push_back(static_cast<char>((chunk >> 8) & 0xFF));
            if (paddingCount < 1)
                decoded.push_back(static_cast<char>(chunk & 0xFF));
        }

        return decoded;
    }
}
