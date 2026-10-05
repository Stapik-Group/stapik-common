#include "SingleInstanceGuard.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/Log.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vector>
#endif

namespace stapik::app
{
#ifdef _WIN32
    namespace
    {
        std::wstring toWide(const std::string& utf8)
        {
            if (utf8.empty())
                return {};

            const auto length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (length <= 0)
                return {};

            std::wstring wide(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
            return wide;
        }
    }

    std::unique_ptr<SingleInstanceGuard> SingleInstanceGuard::acquire(const std::string& applicationId, const std::string& displayName)
    {
        // "Local\" scopes the name to the user's session: two users on one machine may each run a copy.
        const auto name = L"Local\\" + toWide(applicationId);

        const HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
        if (mutex == nullptr)
        {
            // Better to start than to refuse to start because of a failure of the guard itself.
            log::warning("Cannot create the single instance mutex (error {}); starting without it", GetLastError());
            return std::unique_ptr<SingleInstanceGuard>(new SingleInstanceGuard(nullptr));
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(mutex);
            log::info("{} is already running", displayName);

            const auto text = LocaleManager::instance().translate("app.alreadyRunning", { { "name", displayName } });
            MessageBoxW(nullptr, toWide(text).c_str(), toWide(displayName).c_str(), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
            return nullptr;
        }

        return std::unique_ptr<SingleInstanceGuard>(new SingleInstanceGuard(mutex));
    }

    SingleInstanceGuard::~SingleInstanceGuard()
    {
        if (m_mutexHandle != nullptr)
            CloseHandle(static_cast<HANDLE>(m_mutexHandle));
    }
#else
    std::unique_ptr<SingleInstanceGuard> SingleInstanceGuard::acquire(const std::string&, const std::string&)
    {
        return std::unique_ptr<SingleInstanceGuard>(new SingleInstanceGuard(nullptr));
    }

    SingleInstanceGuard::~SingleInstanceGuard() = default;
#endif

    SingleInstanceGuard::SingleInstanceGuard(void* mutexHandle) :
        m_mutexHandle(mutexHandle)
    {}
}
