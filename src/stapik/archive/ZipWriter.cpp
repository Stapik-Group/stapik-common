#include "ZipWriter.hpp"

#include "detail/ZipFormat.hpp"

#include "stapik/storage/PathText.hpp"

#include <zlib.h>

#include <chrono>
#include <limits>
#include <optional>
#include <system_error>
#include <utility>

namespace stapik::archive
{
    namespace
    {
        using namespace detail;

        constexpr std::uint64_t MAX_ARCHIVE_BYTES = std::numeric_limits<std::uint32_t>::max();
        constexpr int DOS_YEAR_BASE = 1980;
        constexpr int DOS_YEAR_LAST = 2107;

        struct DosTimestamp
        {
            std::uint16_t time = 0;
            std::uint16_t date = 0;
        };

        // ZIP stores local time without a zone; UTC is used so that the archive does not depend on the machine.
        DosTimestamp toDosTimestamp(const time::TimePoint moment)
        {
            const auto day = std::chrono::floor<std::chrono::days>(moment);
            const std::chrono::year_month_day date{ day };
            const std::chrono::hh_mm_ss timeOfDay{ std::chrono::floor<std::chrono::seconds>(moment - day) };

            const auto year = static_cast<int>(date.year());
            if (year < DOS_YEAR_BASE || year > DOS_YEAR_LAST)
                return { .time = 0, .date = static_cast<std::uint16_t>((1 << 5) | 1) };

            const auto dosDate = ((year - DOS_YEAR_BASE) << 9) | (static_cast<int>(static_cast<unsigned>(date.month())) << 5) | static_cast<int>(static_cast<unsigned>(date.day()));
            const auto dosTime = (static_cast<int>(timeOfDay.hours().count()) << 11)
                | (static_cast<int>(timeOfDay.minutes().count()) << 5)
                | static_cast<int>(timeOfDay.seconds().count() / 2);

            return { .time = static_cast<std::uint16_t>(dosTime), .date = static_cast<std::uint16_t>(dosDate) };
        }

        std::optional<std::vector<std::uint8_t>> deflateRaw(const std::span<const std::uint8_t> content)
        {
            z_stream stream{};
            if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
                return std::nullopt;

            std::vector<std::uint8_t> compressed(deflateBound(&stream, content.size()));

            stream.next_in = const_cast<Bytef*>(content.data());
            stream.avail_in = static_cast<uInt>(content.size());
            stream.next_out = compressed.data();
            stream.avail_out = static_cast<uInt>(compressed.size());

            const int status = deflate(&stream, Z_FINISH);
            const auto produced = stream.total_out;
            deflateEnd(&stream);

            if (status != Z_STREAM_END)
                return std::nullopt;

            compressed.resize(produced);
            return compressed;
        }
    }

    ArchiveResult<std::unique_ptr<ZipWriter>> ZipWriter::create(std::filesystem::path target, const time::TimePoint modified)
    {
        auto temporary = target;
        temporary += ".tmp";

        std::unique_ptr<ZipWriter> writer(new ZipWriter(std::move(target), std::move(temporary), modified));
        if (auto opened = writer->open(); !opened)
            return Failure(std::move(opened).error());

        return writer;
    }

    ZipWriter::ZipWriter(std::filesystem::path target, std::filesystem::path temporary, const time::TimePoint modified) :
        m_target(std::move(target)),
        m_temporary(std::move(temporary))
    {
        const auto [time, date] = toDosTimestamp(modified);
        m_dosTime = time;
        m_dosDate = date;
    }

    ZipWriter::~ZipWriter()
    {
        if (!m_finished)
            discard();
    }

    ArchiveResult<void> ZipWriter::open()
    {
        std::error_code errorCode;
        if (m_target.has_parent_path())
            std::filesystem::create_directories(m_target.parent_path(), errorCode);
        if (errorCode)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot create the folder of " + storage::pathText(m_target) + ": " + errorCode.message());

        m_stream.open(m_temporary, std::ios::binary | std::ios::trunc);
        if (!m_stream)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot create " + storage::pathText(m_temporary));

        return {};
    }

    ArchiveResult<void> ZipWriter::write(const std::span<const std::uint8_t> bytes)
    {
        m_stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!m_stream)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot write to " + storage::pathText(m_temporary));

        m_written += bytes.size();
        return {};
    }

    void ZipWriter::discard()
    {
        m_stream.close();

        std::error_code errorCode;
        std::filesystem::remove(m_temporary, errorCode);
    }

    ArchiveResult<void> ZipWriter::addText(const std::string_view name, const std::string_view text)
    {
        return add(name, { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
    }

    ArchiveResult<void> ZipWriter::add(const std::string_view name, const std::span<const std::uint8_t> content)
    {
        if (m_records.size() >= MAX_ENTRIES)
            return archiveFailure(ArchiveErrorKind::Unsupported, "The archive cannot hold more than 65534 files");

        if (name.empty() || name.size() > std::numeric_limits<std::uint16_t>::max())
            return archiveFailure(ArchiveErrorKind::Unsupported, "Invalid file name in the archive: " + std::string(name));

        if (content.size() > MAX_ARCHIVE_BYTES || m_written + content.size() > MAX_ARCHIVE_BYTES)
            return archiveFailure(ArchiveErrorKind::Unsupported, "The archive cannot be bigger than 4 GiB");

        CentralRecord record;
        record.name = std::string(name);
        record.size = static_cast<std::uint32_t>(content.size());
        record.crc32 = content.empty() ? 0 : static_cast<std::uint32_t>(crc32(0, content.data(), static_cast<uInt>(content.size())));
        record.localHeaderOffset = static_cast<std::uint32_t>(m_written);

        std::vector<std::uint8_t> deflated;
        auto payload = content;
        record.method = METHOD_STORED;
        if (!content.empty())
        {
            // Photos and other already compressed files do not shrink; storing them saves time when reading.
            if (auto candidate = deflateRaw(content); candidate && candidate->size() < content.size())
            {
                deflated = std::move(*candidate);
                payload = deflated;
                record.method = METHOD_DEFLATED;
            }
        }
        record.compressedSize = static_cast<std::uint32_t>(payload.size());

        std::vector<std::uint8_t> header;
        header.reserve(LOCAL_HEADER_SIZE + name.size());
        appendLe32(header, LOCAL_HEADER_SIGNATURE);
        appendLe16(header, VERSION_NEEDED);
        appendLe16(header, FLAG_UTF8_NAMES);
        appendLe16(header, record.method);
        appendLe16(header, m_dosTime);
        appendLe16(header, m_dosDate);
        appendLe32(header, record.crc32);
        appendLe32(header, record.compressedSize);
        appendLe32(header, record.size);
        appendLe16(header, static_cast<std::uint16_t>(name.size()));
        appendLe16(header, 0);
        header.insert(header.end(), name.begin(), name.end());

        if (auto written = write(header); !written)
            return written;
        if (auto written = write(payload); !written)
            return written;

        m_records.push_back(std::move(record));
        return {};
    }

    ArchiveResult<void> ZipWriter::finish()
    {
        std::vector<std::uint8_t> directory;
        for (const auto&[name, method, crc32, compressedSize, size, localHeaderOffset] : m_records)
        {
            appendLe32(directory, CENTRAL_HEADER_SIGNATURE);
            appendLe16(directory, VERSION_NEEDED);
            appendLe16(directory, VERSION_NEEDED);
            appendLe16(directory, FLAG_UTF8_NAMES);
            appendLe16(directory, method);
            appendLe16(directory, m_dosTime);
            appendLe16(directory, m_dosDate);
            appendLe32(directory, crc32);
            appendLe32(directory, compressedSize);
            appendLe32(directory, size);
            appendLe16(directory, static_cast<std::uint16_t>(name.size()));
            appendLe16(directory, 0);
            appendLe16(directory, 0);
            appendLe16(directory, 0);
            appendLe16(directory, 0);
            appendLe32(directory, 0);
            appendLe32(directory, localHeaderOffset);
            directory.insert(directory.end(), name.begin(), name.end());
        }

        if (m_written + directory.size() + END_OF_DIRECTORY_SIZE > MAX_ARCHIVE_BYTES)
            return archiveFailure(ArchiveErrorKind::Unsupported, "The archive cannot be bigger than 4 GiB");

        const auto directoryOffset = static_cast<std::uint32_t>(m_written);
        const auto directorySize = static_cast<std::uint32_t>(directory.size());
        const auto entryCount = static_cast<std::uint16_t>(m_records.size());

        if (auto written = write(directory); !written)
            return written;

        std::vector<std::uint8_t> end;
        appendLe32(end, END_OF_DIRECTORY_SIGNATURE);
        appendLe16(end, 0);
        appendLe16(end, 0);
        appendLe16(end, entryCount);
        appendLe16(end, entryCount);
        appendLe32(end, directorySize);
        appendLe32(end, directoryOffset);
        appendLe16(end, 0);

        if (auto written = write(end); !written)
            return written;

        m_stream.close();
        if (m_stream.fail())
            return archiveFailure(ArchiveErrorKind::Io, "Cannot finish writing " + storage::pathText(m_temporary));

        std::error_code errorCode;
        std::filesystem::rename(m_temporary, m_target, errorCode);
        if (errorCode)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot move the archive to " + storage::pathText(m_target) + ": " + errorCode.message());

        m_finished = true;
        return {};
    }
}
