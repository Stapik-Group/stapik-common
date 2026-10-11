#include "LogFile.hpp"

#include <chrono>
#include <cstdio>
#include <format>
#include <string>
#include <system_error>

namespace
{
    std::filesystem::path backupPath(const std::filesystem::path& file, const unsigned index)
    {
        auto backup = file;
        backup += "." + std::to_string(index);
        return backup;
    }

    bool hasReachedMaxSize(const std::filesystem::path& file, const std::uintmax_t maxFileSize)
    {
        std::error_code errorCode;
        if (!std::filesystem::is_regular_file(file, errorCode))
            return false;

        const auto size = std::filesystem::file_size(file, errorCode);
        return !errorCode && size >= maxFileSize;
    }

    FILE* reopenStandardError(const std::filesystem::path& file)
    {
#ifdef _WIN32
        return _wfreopen(file.c_str(), L"a", stderr);
#else
        return std::freopen(file.c_str(), "a", stderr);
#endif
    }
}

namespace stapik::log
{
    void rotateLogFiles(const std::filesystem::path& file, const FileRotation rotation)
    {
        if (!hasReachedMaxSize(file, rotation.maxFileSize))
            return;

        std::error_code errorCode;
        const unsigned backupCount = rotation.fileCount > 0 ? rotation.fileCount - 1 : 0;
        if (backupCount == 0)
        {
            std::filesystem::remove(file, errorCode);
            return;
        }

        std::filesystem::remove(backupPath(file, backupCount), errorCode);
        for (unsigned index = backupCount - 1; index >= 1; --index)
        {
            if (std::filesystem::exists(backupPath(file, index), errorCode))
                std::filesystem::rename(backupPath(file, index), backupPath(file, index + 1), errorCode);
        }

        std::filesystem::rename(file, backupPath(file, 1), errorCode);
    }

    bool redirectStandardErrorToFile(const std::filesystem::path& file, const FileRotation rotation)
    {
        rotateLogFiles(file, rotation);

        if (reopenStandardError(file) == nullptr)
            return false;

        std::setvbuf(stderr, nullptr, _IONBF, 0);

        const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        std::fprintf(stderr, "---- started %s UTC ----\n", std::format("{:%F %T}", now).c_str());
        return true;
    }
}
