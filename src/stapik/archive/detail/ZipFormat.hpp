#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// Layout constants of the ZIP format (PKWARE APPNOTE 4.3), shared by ZipWriter and ZipReader.
namespace stapik::archive::detail
{
    inline constexpr std::uint32_t LOCAL_HEADER_SIGNATURE = 0x04034b50;
    inline constexpr std::uint32_t CENTRAL_HEADER_SIGNATURE = 0x02014b50;
    inline constexpr std::uint32_t END_OF_DIRECTORY_SIGNATURE = 0x06054b50;

    inline constexpr std::size_t LOCAL_HEADER_SIZE = 30;
    inline constexpr std::size_t CENTRAL_HEADER_SIZE = 46;
    inline constexpr std::size_t END_OF_DIRECTORY_SIZE = 22;

    inline constexpr std::uint16_t VERSION_NEEDED = 20;
    inline constexpr std::uint16_t FLAG_ENCRYPTED = 1U << 0;
    inline constexpr std::uint16_t FLAG_UTF8_NAMES = 1U << 11;

    inline constexpr std::uint16_t METHOD_STORED = 0;
    inline constexpr std::uint16_t METHOD_DEFLATED = 8;

    // A field holding this value means the real number is in a ZIP64 record, which is not supported.
    inline constexpr std::uint16_t ZIP64_MARKER_16 = 0xFFFF;
    inline constexpr std::uint32_t ZIP64_MARKER_32 = 0xFFFFFFFF;

    template<typename Buffer>
    void appendLe16(Buffer& buffer, const std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    template<typename Buffer>
    void appendLe32(Buffer& buffer, const std::uint32_t value)
    {
        appendLe16(buffer, static_cast<std::uint16_t>(value & 0xFFFF));
        appendLe16(buffer, static_cast<std::uint16_t>(value >> 16));
    }

    [[nodiscard]] inline std::uint16_t readLe16(const std::span<const std::byte> bytes, const std::size_t offset)
    {
        return static_cast<std::uint16_t>(
            std::to_integer<std::uint16_t>(bytes[offset]) | (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8));
    }

    [[nodiscard]] inline std::uint32_t readLe32(const std::span<const std::byte> bytes, const std::size_t offset)
    {
        return static_cast<std::uint32_t>(readLe16(bytes, offset)) |
            (static_cast<std::uint32_t>(readLe16(bytes, offset + 2)) << 16);
    }
}
