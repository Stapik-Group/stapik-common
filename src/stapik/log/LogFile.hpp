#pragma once

#include <cstdint>
#include <filesystem>

namespace stapik::log
{
    struct FileRotation
    {
        std::uintmax_t maxFileSize;
        unsigned fileCount;
    };

    // The current file plus four backups (<name>.1 is the newest), at most 5 MiB on disk.
    inline constexpr FileRotation DEFAULT_FILE_ROTATION{std::uintmax_t{1} << 20, 5};

    // Shifts <file> to <file>.1, <file>.1 to <file>.2 and so on, dropping the oldest, once <file> has reached the
    // maximum size. Does nothing for a smaller or missing file.
    void rotateLogFiles(const std::filesystem::path& file, FileRotation rotation = DEFAULT_FILE_ROTATION);
}
