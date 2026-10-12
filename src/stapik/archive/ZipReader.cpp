#include "ZipReader.hpp"

#include "detail/ZipFormat.hpp"

#include "stapik/storage/PathText.hpp"

#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <span>
#include <system_error>
#include <utility>

namespace stapik::archive
{
    namespace
    {
        using namespace detail;

        constexpr std::uint64_t MAX_COMMENT_BYTES = std::numeric_limits<std::uint16_t>::max();

        bool readAt(std::ifstream& file, const std::uint64_t offset, std::uint8_t* destination, const std::size_t count)
        {
            file.clear();
            file.seekg(static_cast<std::streamoff>(offset));
            file.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(count));
            return static_cast<bool>(file) && static_cast<std::size_t>(file.gcount()) == count;
        }

        std::vector<std::uint8_t> readBlock(std::ifstream& file, const std::uint64_t offset, const std::size_t count, bool& ok)
        {
            std::vector<std::uint8_t> block(count);
            ok = count == 0 || readAt(file, offset, block.data(), count);
            return block;
        }

        ArchiveResult<std::vector<std::uint8_t>> inflateRaw(const std::vector<std::uint8_t>& compressed, const std::uint32_t size)
        {
            std::vector<std::uint8_t> content(size);
            if (size == 0)
                return content;

            z_stream stream{};
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                return archiveFailure(ArchiveErrorKind::Io, "Cannot start decompression");

            stream.next_in = const_cast<Bytef*>(compressed.data());
            stream.avail_in = static_cast<uInt>(compressed.size());
            stream.next_out = content.data();
            stream.avail_out = size;

            // The output buffer has exactly the declared size, so a stream that wants more room is rejected here.
            const int status = inflate(&stream, Z_FINISH);
            const bool complete = status == Z_STREAM_END && stream.total_out == size;
            inflateEnd(&stream);

            if (!complete)
                return archiveFailure(ArchiveErrorKind::Corrupted, "Compressed data does not match the declared size");

            return content;
        }
    }

    ZipReader::ZipReader(std::filesystem::path source, const std::uint64_t fileSize, std::vector<ZipEntry> entries) :
        m_source(std::move(source)),
        m_fileSize(fileSize),
        m_entries(std::move(entries))
    {}

    ArchiveResult<ZipReader> ZipReader::open(std::filesystem::path source)
    {
        std::ifstream file(source, std::ios::binary);
        if (!file)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot open " + storage::pathText(source));

        std::error_code errorCode;
        const auto fileSize = std::filesystem::file_size(source, errorCode);
        if (errorCode)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot read the size of " + storage::pathText(source) + ": " + errorCode.message());

        const auto notAnArchive = [&source]
        {
            return archiveFailure(ArchiveErrorKind::NotAnArchive, storage::pathText(source) + " is not a ZIP archive");
        };

        if (fileSize < END_OF_DIRECTORY_SIZE)
            return notAnArchive();

        const auto tailSize = std::min<std::uint64_t>(fileSize, END_OF_DIRECTORY_SIZE + MAX_COMMENT_BYTES);
        const auto tailOffset = fileSize - tailSize;

        bool ok = false;
        const auto tail = readBlock(file, tailOffset, tailSize, ok);
        if (!ok)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot read " + storage::pathText(source));

        // The comment at the end has a variable length, so the record is looked for backwards.
        std::size_t endPosition = tail.size();
        for (std::size_t position = tail.size() - END_OF_DIRECTORY_SIZE + 1; position-- > 0;)
        {
            if (readLe32(tail, position) != END_OF_DIRECTORY_SIGNATURE)
                continue;

            if (const auto commentLength = readLe16(tail, position + 20); position + END_OF_DIRECTORY_SIZE + commentLength == tail.size())
            {
                endPosition = position;
                break;
            }
        }

        if (endPosition == tail.size())
            return notAnArchive();

        const auto diskNumber = readLe16(tail, endPosition + 4);
        const auto directoryDisk = readLe16(tail, endPosition + 6);
        const auto entriesOnDisk = readLe16(tail, endPosition + 8);
        const auto entryCount = readLe16(tail, endPosition + 10);
        const auto directorySize = readLe32(tail, endPosition + 12);
        const auto directoryOffset = readLe32(tail, endPosition + 16);

        if (entryCount == ZIP64_MARKER_16 || directorySize == ZIP64_MARKER_32 || directoryOffset == ZIP64_MARKER_32)
            return archiveFailure(ArchiveErrorKind::Unsupported, "ZIP64 archives are not supported");

        if (diskNumber != 0 || directoryDisk != 0 || entriesOnDisk != entryCount)
            return archiveFailure(ArchiveErrorKind::Unsupported, "Archives split into several files are not supported");

        if (static_cast<std::uint64_t>(directoryOffset) + directorySize > tailOffset + endPosition)
            return archiveFailure(ArchiveErrorKind::Corrupted, "The directory of the archive is outside the file");

        const auto directory = readBlock(file, directoryOffset, directorySize, ok);
        if (!ok)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot read " + storage::pathText(source));

        std::vector<ZipEntry> entries;
        std::set<std::string> names;
        std::size_t position = 0;
        for (std::size_t index = 0; index < entryCount; ++index)
        {
            const auto corrupted = [&source]
            {
                return archiveFailure(ArchiveErrorKind::Corrupted, "The directory of " + storage::pathText(source) + " is damaged");
            };

            if (position + CENTRAL_HEADER_SIZE > directory.size() || readLe32(directory, position) != CENTRAL_HEADER_SIGNATURE)
                return corrupted();

            const auto flags = readLe16(directory, position + 8);
            const auto nameLength = readLe16(directory, position + 28);
            const auto extraLength = readLe16(directory, position + 30);
            const auto commentLength = readLe16(directory, position + 32);
            const auto recordSize = CENTRAL_HEADER_SIZE + nameLength + extraLength + commentLength;
            if (position + recordSize > directory.size())
                return corrupted();

            ZipEntry entry;
            entry.name.assign(reinterpret_cast<const char*>(directory.data()) + position + CENTRAL_HEADER_SIZE, nameLength);
            entry.method = readLe16(directory, position + 10);
            entry.crc32 = readLe32(directory, position + 16);
            entry.compressedSize = readLe32(directory, position + 20);
            entry.size = readLe32(directory, position + 24);
            entry.localHeaderOffset = readLe32(directory, position + 42);
            position += recordSize;

            if (entry.compressedSize == ZIP64_MARKER_32 || entry.size == ZIP64_MARKER_32 || entry.localHeaderOffset == ZIP64_MARKER_32)
                return archiveFailure(ArchiveErrorKind::Unsupported, "ZIP64 archives are not supported");

            if ((flags & FLAG_ENCRYPTED) != 0)
                return archiveFailure(ArchiveErrorKind::Unsupported, "Encrypted archives are not supported");

            if (entry.name.ends_with('/'))
                continue;

            if (entry.method != METHOD_STORED && entry.method != METHOD_DEFLATED)
                return archiveFailure(ArchiveErrorKind::Unsupported, "Unsupported compression method in " + storage::pathText(source));

            if (!names.insert(entry.name).second)
                return archiveFailure(ArchiveErrorKind::Corrupted, "The archive lists \"" + entry.name + "\" twice");

            entries.push_back(std::move(entry));
        }

        return ZipReader(std::move(source), fileSize, std::move(entries));
    }

    const std::vector<ZipEntry>& ZipReader::entries() const
    {
        return m_entries;
    }

    const ZipEntry* ZipReader::find(const std::string_view name) const
    {
        const auto found = std::ranges::find(m_entries, name, &ZipEntry::name);
        return found == m_entries.end() ? nullptr : &*found;
    }

    ArchiveResult<std::vector<std::uint8_t>> ZipReader::read(const std::string_view name) const
    {
        const auto* entry = find(name);
        if (entry == nullptr)
            return archiveFailure(ArchiveErrorKind::Corrupted, "The archive lacks \"" + std::string(name) + "\"");

        return read(*entry);
    }

    ArchiveResult<std::string> ZipReader::readText(const std::string_view name) const
    {
        auto bytes = read(name);
        if (!bytes)
            return Failure(std::move(bytes).error());

        return std::string(bytes->begin(), bytes->end());
    }

    ArchiveResult<std::vector<std::uint8_t>> ZipReader::read(const ZipEntry& entry) const
    {
        const auto damaged = [&entry]
        {
            return archiveFailure(ArchiveErrorKind::Corrupted, "\"" + entry.name + "\" is damaged in the archive");
        };

        if (entry.size > MAX_ENTRY_BYTES)
            return archiveFailure(ArchiveErrorKind::Unsupported, "\"" + entry.name + "\" is too big to be read");

        std::ifstream file(m_source, std::ios::binary);
        if (!file)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot open " + storage::pathText(m_source));

        std::vector<std::uint8_t> localHeader(LOCAL_HEADER_SIZE);
        if (!readAt(file, entry.localHeaderOffset, localHeader.data(), localHeader.size())
            || readLe32(localHeader, 0) != LOCAL_HEADER_SIGNATURE)
            return damaged();

        // The lengths of the name and the extra field may differ from the ones in the directory.
        const std::uint64_t dataOffset = static_cast<std::uint64_t>(entry.localHeaderOffset) + LOCAL_HEADER_SIZE
            + readLe16(localHeader, 26) + readLe16(localHeader, 28);
        if (dataOffset + entry.compressedSize > m_fileSize)
            return damaged();

        bool ok = false;
        auto stored = readBlock(file, dataOffset, entry.compressedSize, ok);
        if (!ok)
            return archiveFailure(ArchiveErrorKind::Io, "Cannot read " + storage::pathText(m_source));

        std::vector<std::uint8_t> content;
        if (entry.method == METHOD_STORED)
        {
            if (entry.compressedSize != entry.size)
                return damaged();
            content = std::move(stored);
        }
        else
        {
            auto inflated = inflateRaw(stored, entry.size);
            if (!inflated)
                return damaged();
            content = std::move(*inflated);
        }

        if (const auto crc = content.empty() ? 0UL : ::crc32(0, content.data(), static_cast<uInt>(content.size())); static_cast<std::uint32_t>(crc) != entry.crc32)
            return damaged();

        return content;
    }
}
