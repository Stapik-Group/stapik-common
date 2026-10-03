#include "AtomicFile.hpp"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "stapik/log/Log.hpp"

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

    void logFailure(const std::filesystem::path& path, const char* step, const int errorNumber)
    {
        stapik::log::warning("Atomic write of {} failed at {}: {}", path.string(), step, std::strerror(errorNumber));
    }
}

namespace stapik::storage
{
    bool writeFileAtomically(const std::filesystem::path &path, const std::string_view content, std::filesystem::perms permissions)
    {
        const auto directory = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");

        std::error_code errorCode;
        std::filesystem::create_directories(directory, errorCode);
        if (errorCode)
        {
            log::warning("Cannot create directory {}: {}", directory.string(), errorCode.message());
            return false;
        }

        auto temporaryPath = (directory / (path.filename().string() + ".tmp.XXXXXX")).string();

        const int fileDescriptor = mkstemp(temporaryPath.data());
        if (fileDescriptor < 0)
        {
            logFailure(path, "mkstemp", errno);
            return false;
        }

        const auto fail = [&](const char* step)
        {
            const int savedErrno = errno;
            close(fileDescriptor);
            unlink(temporaryPath.c_str());
            logFailure(path, step, savedErrno);
            return false;
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
            logFailure(path, "close", savedErrno);
            return false;
        }

        if (rename(temporaryPath.c_str(), path.c_str()) != 0)
        {
            const int savedErrno = errno;
            unlink(temporaryPath.c_str());
            logFailure(path, "rename", savedErrno);
            return false;
        }

        syncDirectory(directory);
        return true;
    }
}
