#pragma once

#include "ArchiveError.hpp"

#include "stapik/time/Clock.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::archive
{
    class ZipWriter
    {
    public:
        static constexpr std::size_t MAX_ENTRIES = 65534;

        [[nodiscard]] static ArchiveResult<std::unique_ptr<ZipWriter> > create(
            std::filesystem::path target, time::TimePoint modified);

        ~ZipWriter();
        ZipWriter(const ZipWriter &) = delete;
        ZipWriter &operator=(const ZipWriter &) = delete;

        [[nodiscard]] ArchiveResult<void> add(std::string_view name, std::span<const std::uint8_t> content);
        [[nodiscard]] ArchiveResult<void> addText(std::string_view name, std::string_view text);
        [[nodiscard]] ArchiveResult<void> finish();

    private:
        struct CentralRecord
        {
            std::string name;
            std::uint16_t method = 0;
            std::uint32_t crc32 = 0;
            std::uint32_t compressedSize = 0;
            std::uint32_t size = 0;
            std::uint32_t localHeaderOffset = 0;
        };

        ZipWriter(std::filesystem::path target, std::filesystem::path temporary, time::TimePoint modified);

        [[nodiscard]] ArchiveResult<void> open();
        [[nodiscard]] ArchiveResult<void> write(std::span<const std::uint8_t> bytes);

        void discard();

        std::filesystem::path m_target;
        std::filesystem::path m_temporary;
        std::uint16_t m_dosTime = 0;
        std::uint16_t m_dosDate = 0;
        std::ofstream m_stream;
        std::uint64_t m_written = 0;
        std::vector<CentralRecord> m_records;
        bool m_finished = false;
    };
}
