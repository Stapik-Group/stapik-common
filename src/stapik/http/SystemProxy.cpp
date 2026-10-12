#include "SystemProxy.hpp"

#ifdef _WIN32
#include "ProxyList.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#endif

namespace stapik::http
{
#ifdef _WIN32
    namespace
    {
        constexpr std::chrono::seconds CACHE_LIFETIME{ 60 };
        constexpr int RESOLVE_TIMEOUT_MILLISECONDS = 5000;

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

        std::string toUtf8(const wchar_t* wide)
        {
            if (wide == nullptr)
                return {};

            const auto length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 1)
                return {};

            std::string utf8(static_cast<std::size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), length, nullptr, nullptr);
            utf8.pop_back();
            return utf8;
        }

        void freeIfSet(wchar_t* text)
        {
            if (text != nullptr)
                GlobalFree(text);
        }

        class UserProxyConfig
        {
        public:
            UserProxyConfig() : m_loaded(WinHttpGetIEProxyConfigForCurrentUser(&m_config) == TRUE)
            {
            }

            ~UserProxyConfig()
            {
                freeIfSet(m_config.lpszProxy);
                freeIfSet(m_config.lpszProxyBypass);
                freeIfSet(m_config.lpszAutoConfigUrl);
            }

            UserProxyConfig(const UserProxyConfig&) = delete;
            UserProxyConfig& operator=(const UserProxyConfig&) = delete;

            [[nodiscard]] bool loaded() const { return m_loaded; }
            [[nodiscard]] const WINHTTP_CURRENT_USER_IE_PROXY_CONFIG& get() const { return m_config; }

        private:
            WINHTTP_CURRENT_USER_IE_PROXY_CONFIG m_config{};
            bool m_loaded;
        };

        std::optional<SystemProxy> proxyOf(const std::string& url, const std::string& proxyList, const std::string& bypassList)
        {
            const auto proxy = proxyForScheme(proxyList, schemeOf(url));
            if (!proxy)
                return std::nullopt;

            return SystemProxy{ .url = *proxy, .bypass = noProxyListOf(bypassList) };
        }

        std::optional<SystemProxy> resolveWithScript(const std::string& url, const WINHTTP_CURRENT_USER_IE_PROXY_CONFIG& config)
        {
            const std::unique_ptr<void, decltype(&WinHttpCloseHandle)> session(
                WinHttpOpen(L"stapik", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
                &WinHttpCloseHandle);
            if (!session)
                return std::nullopt;

            WinHttpSetTimeouts(session.get(), RESOLVE_TIMEOUT_MILLISECONDS, RESOLVE_TIMEOUT_MILLISECONDS, RESOLVE_TIMEOUT_MILLISECONDS, RESOLVE_TIMEOUT_MILLISECONDS);

            WINHTTP_AUTOPROXY_OPTIONS options{};
            if (config.fAutoDetect)
            {
                options.dwFlags |= WINHTTP_AUTOPROXY_AUTO_DETECT;
                options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
            }
            if (config.lpszAutoConfigUrl != nullptr)
            {
                options.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
                options.lpszAutoConfigUrl = config.lpszAutoConfigUrl;
            }
            options.fAutoLogonIfChallenged = TRUE;

            WINHTTP_PROXY_INFO info{};
            if (!WinHttpGetProxyForUrl(session.get(), toWide(url).c_str(), &options, &info))
                return std::nullopt;

            const auto proxyList = toUtf8(info.lpszProxy);
            const auto bypassList = toUtf8(info.lpszProxyBypass);
            freeIfSet(info.lpszProxy);
            freeIfSet(info.lpszProxyBypass);

            return proxyOf(url, proxyList, bypassList);
        }

        std::optional<SystemProxy> resolve(const std::string& url)
        {
            const UserProxyConfig config;
            if (!config.loaded())
                return std::nullopt;

            const auto& settings = config.get();
            if (settings.fAutoDetect || settings.lpszAutoConfigUrl != nullptr)
            {
                if (auto proxy = resolveWithScript(url, settings))
                    return proxy;
            }

            return proxyOf(url, toUtf8(settings.lpszProxy), toUtf8(settings.lpszProxyBypass));
        }

        struct CacheEntry
        {
            std::optional<SystemProxy> proxy;
            std::chrono::steady_clock::time_point resolvedAt;
        };
    }

    std::optional<SystemProxy> systemProxyFor(const std::string& url)
    {
        static std::mutex mutex;
        static std::map<std::string, CacheEntry> cache;

        const std::lock_guard lock(mutex);
        const std::string origin(originOf(url));
        const auto now = std::chrono::steady_clock::now();

        if (const auto found = cache.find(origin); found != cache.end() && now - found->second.resolvedAt < CACHE_LIFETIME)
            return found->second.proxy;

        auto proxy = resolve(url);
        cache[origin] = CacheEntry{ .proxy = proxy, .resolvedAt = now };
        return proxy;
    }
#else
    std::optional<SystemProxy> systemProxyFor(const std::string&)
    {
        return std::nullopt;
    }
#endif
}
