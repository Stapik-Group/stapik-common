#pragma once

#include <memory>
#include <string>

namespace stapik::app
{
    class SingleInstanceGuard
    {
    public:
        [[nodiscard]] static std::unique_ptr<SingleInstanceGuard> acquire(const std::string& applicationId, const std::string& displayName);

        ~SingleInstanceGuard();

        SingleInstanceGuard(const SingleInstanceGuard&) = delete;
        SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

    private:
        explicit SingleInstanceGuard(void* mutexHandle);

        void* m_mutexHandle;
    };
}
