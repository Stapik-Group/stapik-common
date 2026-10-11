#pragma once

#include <filesystem>
#include <memory>

namespace stapik::storage
{
    // Advisory lock that keeps two processes from working on the same user data directory. It is released when
    // the object is destroyed or the process ends, however it ends.
    class DirectoryLock
    {
    public:
        // Returns nullptr only when another process holds the lock. When the lock cannot be taken for any other
        // reason (read-only directory, a file system without locks) the result is an object that holds nothing:
        // better to start than to refuse to start because of the lock itself.
        [[nodiscard]] static std::unique_ptr<DirectoryLock> tryAcquire(const std::filesystem::path& directory);

        ~DirectoryLock();

        DirectoryLock(const DirectoryLock&) = delete;
        DirectoryLock& operator=(const DirectoryLock&) = delete;

    private:
        explicit DirectoryLock(int descriptor);

        int m_descriptor;
    };
}
