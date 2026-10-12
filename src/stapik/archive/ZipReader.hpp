#pragma once

#include "ArchiveError.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::archive
{
    struct ZipEntry
    {
        std::string name;
        std::uint16_t method = 0;
        std::uint32_t crc32 = 0;
        std::uint32_t compressedSize = 0;
        std::uint32_t size = 0;
        std::uint32_t localHeaderOffset = 0;
    };

    class ZipReader
    {
    public:
        // A bigger entry is refused instead of read: its declared size comes from the file and cannot be trusted.
        static constexpr std::uint32_t MAX_ENTRY_BYTES = 512U * 1024U * 1024U;

        [[nodiscard]] static ArchiveResult<ZipReader> open(std::filesystem::path source);

        [[nodiscard]] const std::vector<ZipEntry> &entries() const;
        [[nodiscard]] const ZipEntry *find(std::string_view name) const;
        [[nodiscard]] ArchiveResult<std::vector<std::uint8_t> > read(const ZipEntry &entry) const;
        [[nodiscard]] ArchiveResult<std::vector<std::uint8_t> > read(std::string_view name) const;
        [[nodiscard]] ArchiveResult<std::string> readText(std::string_view name) const;

    private:
        ZipReader(std::filesystem::path source, std::uint64_t fileSize, std::vector<ZipEntry> entries);

        std::filesystem::path m_source;
        std::uint64_t m_fileSize;
        std::vector<ZipEntry> m_entries;
    };
}
