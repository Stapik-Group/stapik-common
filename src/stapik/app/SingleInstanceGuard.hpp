#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace stapik::app
{
    // Keeps one instance of an application per user session on Windows: a later instance hands its files to
    // the first one and exits. Elsewhere GApplication does that, and the guard never gets in the way.
    class SingleInstanceGuard
    {
    public:
        using SecondInstanceHandler = std::function<void(const std::vector<std::filesystem::path>& files)>;

        // Returns nullptr when another instance is running and the caller should exit. Otherwise the handler is
        // called on the main thread each time a later instance is started.
        [[nodiscard]] static std::unique_ptr<SingleInstanceGuard> acquire(const std::string& applicationId,
            const std::string& displayName, const std::vector<std::filesystem::path>& files, SecondInstanceHandler onSecondInstance);

        ~SingleInstanceGuard();

        SingleInstanceGuard(const SingleInstanceGuard&) = delete;
        SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

    private:
        struct Impl;

        explicit SingleInstanceGuard(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> m_impl;
    };
}
