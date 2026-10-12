#include "DirectoryLock.hpp"

#include "PathText.hpp"

#include "stapik/log/Log.hpp"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace stapik::storage
{
#ifndef _WIN32
    namespace
    {
        constexpr auto LOCK_FILE_NAME = "instance.lock";
    }

    std::unique_ptr<DirectoryLock> DirectoryLock::tryAcquire(const std::filesystem::path& directory)
    {
        const auto path = directory / LOCK_FILE_NAME;
        const auto holdsNothing = [] { return std::unique_ptr<DirectoryLock>(new DirectoryLock(-1)); };

        const int descriptor = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (descriptor < 0)
        {
            log::warning("Cannot open the lock file {} ({}); continuing without it", pathText(path), std::strerror(errno));
            return holdsNothing();
        }

        if (flock(descriptor, LOCK_EX | LOCK_NB) != 0)
        {
            const int error = errno;
            close(descriptor);

            if (error == EWOULDBLOCK)
                return nullptr;

            log::warning("Cannot lock {} ({}); continuing without it", pathText(path), std::strerror(error));
            return holdsNothing();
        }

        return std::unique_ptr<DirectoryLock>(new DirectoryLock(descriptor));
    }

    DirectoryLock::~DirectoryLock()
    {
        if (m_descriptor >= 0)
            close(m_descriptor);
    }
#else
    // On Windows SingleInstanceGuard keeps the instances apart.
    std::unique_ptr<DirectoryLock> DirectoryLock::tryAcquire(const std::filesystem::path&)
    {
        return std::unique_ptr<DirectoryLock>(new DirectoryLock(-1));
    }

    DirectoryLock::~DirectoryLock() = default;
#endif

    DirectoryLock::DirectoryLock(const int descriptor) :
        m_descriptor(descriptor)
    {}
}
