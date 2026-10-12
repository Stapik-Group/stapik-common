#include "AtomicFile.hpp"

#include <cerrno>
#include <string>
#include <system_error>
#include <utility>

#include "stapik/log/Log.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <format>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    stapik::Failure<stapik::storage::FileError> logAndFail(const std::filesystem::path& path, const char* step, const std::error_code& errorCode)
    {
        stapik::storage::FileError error{ .path = path, .step = step, .code = errorCode };
        stapik::log::warning("Atomic write: {}", error.message());
        return stapik::Failure(std::move(error));
    }
}

#ifdef _WIN32
namespace
{
    constexpr int MAX_TEMPORARY_NAME_ATTEMPTS = 100;
    constexpr int MAX_REPLACE_ATTEMPTS = 5;
    constexpr DWORD REPLACE_RETRY_DELAY_MILLISECONDS = 50;
    constexpr std::size_t MAX_WRITE_CHUNK = std::size_t{ 1 } << 30;

    stapik::Failure<stapik::storage::FileError> logAndFail(const std::filesystem::path& path, const char* step, const DWORD errorCode)
    {
        return logAndFail(path, step, std::error_code(static_cast<int>(errorCode), std::system_category()));
    }

    // CREATE_NEW makes the creation exclusive, so a name that is already taken is simply skipped.
    HANDLE createTemporaryFile(const std::filesystem::path& directory, const std::filesystem::path& targetName, std::filesystem::path& temporaryPath, DWORD& errorCode)
    {
        static std::atomic<unsigned> counter{ 0 };

        for (int attempt = 0; attempt < MAX_TEMPORARY_NAME_ATTEMPTS; ++attempt)
        {
            temporaryPath = directory / (targetName.wstring() + std::format(L".tmp.{:x}.{:x}.{:x}", GetCurrentProcessId(), GetTickCount64(), counter.fetch_add(1)));

            const HANDLE handle = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle != INVALID_HANDLE_VALUE)
                return handle;

            errorCode = GetLastError();
            if (errorCode != ERROR_FILE_EXISTS && errorCode != ERROR_ALREADY_EXISTS)
                return INVALID_HANDLE_VALUE;
        }

        return INVALID_HANDLE_VALUE;
    }

    bool writeAll(const HANDLE handle, std::string_view content, DWORD& errorCode)
    {
        while (!content.empty())
        {
            const auto chunk = static_cast<DWORD>(std::min(content.size(), MAX_WRITE_CHUNK));
            DWORD written = 0;
            if (!WriteFile(handle, content.data(), chunk, &written, nullptr))
            {
                errorCode = GetLastError();
                return false;
            }

            content.remove_prefix(written);
        }

        return true;
    }

    // Antivirus scanners and backup tools briefly lock files they have just seen, which makes the replace fail
    // with "access denied" or "sharing violation" although nothing is wrong.
    bool replaceFile(const std::filesystem::path& temporaryPath, const std::filesystem::path& path, DWORD& errorCode)
    {
        for (int attempt = 0; attempt < MAX_REPLACE_ATTEMPTS; ++attempt)
        {
            if (MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return true;

            errorCode = GetLastError();
            if (errorCode != ERROR_ACCESS_DENIED && errorCode != ERROR_SHARING_VIOLATION && errorCode != ERROR_LOCK_VIOLATION)
                return false;

            Sleep(REPLACE_RETRY_DELAY_MILLISECONDS);
        }

        return false;
    }
}
#else
namespace
{
    bool writeAll(const int fileDescriptor, std::string_view content)
    {
        while (!content.empty())
        {
            const auto written = write(fileDescriptor, content.data(), content.size());
            if (written < 0)
            {
                if (errno == EINTR)
                    continue;
                return false;
            }
            content.remove_prefix(static_cast<std::size_t>(written));
        }
        return true;
    }

    void syncDirectory(const std::filesystem::path& directory)
    {
        const int directoryDescriptor = open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directoryDescriptor < 0)
            return;

        fsync(directoryDescriptor);
        close(directoryDescriptor);
    }

    stapik::Failure<stapik::storage::FileError> logAndFail(const std::filesystem::path& path, const char* step, const int errorNumber)
    {
        return logAndFail(path, step, std::error_code(errorNumber, std::generic_category()));
    }
}
#endif

namespace stapik::storage
{
    FileResult writeFileAtomically(const std::filesystem::path &path, const std::string_view content, std::filesystem::perms permissions)
    {
        const auto directory = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");

        std::error_code errorCode;
        std::filesystem::create_directories(directory, errorCode);
        if (errorCode)
            return logAndFail(directory, "create directory", errorCode);

#ifdef _WIN32
        // Windows has no POSIX permission bits: the file inherits the access rules of the user's profile folder,
        // so `permissions` is accepted for compatibility and ignored.
        static_cast<void>(permissions);

        std::filesystem::path temporaryPath;
        DWORD windowsError = 0;
        const HANDLE handle = createTemporaryFile(directory, path.filename(), temporaryPath, windowsError);
        if (handle == INVALID_HANDLE_VALUE)
            return logAndFail(path, "create temporary file", windowsError);

        const auto failAndCleanUp = [&](const char* step, const DWORD failure, const bool handleStillOpen)
        {
            if (handleStillOpen)
                CloseHandle(handle);

            DeleteFileW(temporaryPath.c_str());
            return logAndFail(path, step, failure);
        };

        if (!writeAll(handle, content, windowsError))
            return failAndCleanUp("write", windowsError, true);

        if (!FlushFileBuffers(handle))
            return failAndCleanUp("flush", GetLastError(), true);

        if (!CloseHandle(handle))
            return failAndCleanUp("close", GetLastError(), false);

        if (!replaceFile(temporaryPath, path, windowsError))
            return failAndCleanUp("replace", windowsError, false);

        return {};
#else
        auto temporaryPath = (directory / (path.filename().string() + ".tmp.XXXXXX")).string();

        const int fileDescriptor = mkstemp(temporaryPath.data());
        if (fileDescriptor < 0)
            return logAndFail(path, "mkstemp", errno);

        const auto fail = [&](const char* step)
        {
            const int savedErrno = errno;
            close(fileDescriptor);
            unlink(temporaryPath.c_str());
            return logAndFail(path, step, savedErrno);
        };

        if (fchmod(fileDescriptor, static_cast<mode_t>(permissions)) != 0)
            return fail("fchmod");

        if (!writeAll(fileDescriptor, content))
            return fail("write");

        if (fsync(fileDescriptor) != 0)
            return fail("fsync");

        if (close(fileDescriptor) != 0)
        {
            const int savedErrno = errno;
            unlink(temporaryPath.c_str());
            return logAndFail(path, "close", savedErrno);
        }

        if (rename(temporaryPath.c_str(), path.c_str()) != 0)
        {
            const int savedErrno = errno;
            unlink(temporaryPath.c_str());
            return logAndFail(path, "rename", savedErrno);
        }

        syncDirectory(directory);
        return {};
#endif
    }
}
