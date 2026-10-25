#include "AppPaths.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/PathText.hpp"

#include <climits>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    enum class UserDirectory
    {
        Data,
        Config,
        Cache
    };

    std::mutex& overrideMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::optional<std::filesystem::path>& overrideSlot()
    {
        static std::optional<std::filesystem::path> slot;
        return slot;
    }

    const char* subdirectoryName(const UserDirectory kind)
    {
        switch (kind)
        {
            using enum UserDirectory;
            case Data: return "data";
            case Config: return "config";
            case Cache: return "cache";
        }

        return "";
    }

    const char* xdgVariable(const UserDirectory kind)
    {
        switch (kind)
        {
            using enum UserDirectory;
            case Data: return "XDG_DATA_HOME";
            case Config: return "XDG_CONFIG_HOME";
            case Cache: return "XDG_CACHE_HOME";
        }

        return "";
    }

    std::filesystem::path temporaryDirectoryFallback(const std::string& appName, const std::string_view missingVariable)
    {
        static std::once_flag warned;
        std::call_once(warned, [&]
        {
            stapik::log::warning("{} is not set, so the files of {} are kept in the temporary directory and may be lost", missingVariable, appName);
        });

        return std::filesystem::temp_directory_path() / appName;
    }

#ifdef _WIN32
    std::filesystem::path environmentPath(const wchar_t* name)
    {
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0)
            return {};

        std::wstring value(required, L'\0');
        const DWORD length = GetEnvironmentVariableW(name, value.data(), required);
        if (length == 0 || length >= required)
            return {};

        value.resize(length);
        return std::filesystem::path(value);
    }

    // %APPDATA%\<app>\{data,config} for what the user created, %LOCALAPPDATA%\<app>\cache for what can be rebuilt.
    std::filesystem::path platformDefaultDir(const UserDirectory kind, const std::string& appName)
    {
        const auto base = environmentPath(kind == UserDirectory::Cache ? L"LOCALAPPDATA" : L"APPDATA");
        if (base.empty())
            return temporaryDirectoryFallback(appName, kind == UserDirectory::Cache ? "LOCALAPPDATA" : "APPDATA");

        switch (kind)
        {
            case UserDirectory::Data: return base / appName / "data";
            case UserDirectory::Config: return base / appName / "config";
            case UserDirectory::Cache: return base / appName / "cache";
        }

        return base / appName;
    }
#else
    std::filesystem::path platformDefaultDir(const UserDirectory kind, const std::string& appName)
    {
        const auto* home = std::getenv("HOME");
        if (home == nullptr || *home == '\0')
            return temporaryDirectoryFallback(appName, "HOME");

        switch (kind)
        {
            using enum UserDirectory;
            case Data: return std::filesystem::path(home) / ".local/share" / appName;
            case Config: return std::filesystem::path(home) / ".config" / appName;
            case Cache: return std::filesystem::path(home) / ".cache" / appName;
        }

        return std::filesystem::path(home) / appName;
    }
#endif

    // The XDG variables are honoured on every platform: they let tests and portable setups redirect the data.
    std::filesystem::path userDir(const UserDirectory kind, const std::string& appName)
    {
        if (const auto root = AppPaths::rootOverride())
            return *root / subdirectoryName(kind);

        if (const auto* xdgValue = std::getenv(xdgVariable(kind)); xdgValue != nullptr && *xdgValue != '\0')
            return std::filesystem::path(xdgValue) / appName;

        return platformDefaultDir(kind, appName);
    }
}

std::filesystem::path AppPaths::resourcesDir()
{
    return resolveResourcesDir(executableDir());
}

std::filesystem::path AppPaths::resolveResourcesDir(const std::filesystem::path& executableDir)
{
    std::error_code errorCode;
    if (const auto siblingResources = executableDir / "resources"; std::filesystem::exists(siblingResources, errorCode))
        return siblingResources;

    return executableDir.parent_path() / "resources";
}

std::filesystem::path AppPaths::commonResourcesDir()
{
#ifdef STAPIK_COMMON_SOURCE_RESOURCES_DIR
    const std::filesystem::path sourceTreeDir = STAPIK_COMMON_SOURCE_RESOURCES_DIR;
#else
    const std::filesystem::path sourceTreeDir;
#endif

    return resolveCommonResourcesDir(executableDir(), sourceTreeDir);
}

std::filesystem::path AppPaths::resolveCommonResourcesDir(
    const std::filesystem::path& executableDir,
    const std::filesystem::path& sourceTreeDir)
{
    std::vector candidates = {
        executableDir / "resources" / "stapik-common",
        executableDir.parent_path() / "share" / "stapik-common"
    };

    if (!sourceTreeDir.empty())
        candidates.push_back(sourceTreeDir);

    std::error_code errorCode;
    for (const auto& candidate : candidates)
    {
        if (std::filesystem::is_directory(candidate, errorCode))
            return candidate;
    }

    return candidates.front();
}

std::filesystem::path AppPaths::userDataDir(const std::string& appName)
{
    return userDir(UserDirectory::Data, appName);
}

std::filesystem::path AppPaths::userConfigDir(const std::string& appName)
{
    return userDir(UserDirectory::Config, appName);
}

std::filesystem::path AppPaths::userCacheDir(const std::string& appName)
{
    return userDir(UserDirectory::Cache, appName);
}

std::filesystem::path AppPaths::ensureUserDataDir(const std::string& appName)
{
    const auto directory = userDataDir(appName);
    std::error_code errorCode;
    std::filesystem::create_directories(directory, errorCode);
    if (errorCode)
        stapik::log::warning("Cannot create user data directory {}: {}", stapik::storage::pathText(directory), errorCode.message());

    return directory;
}

void AppPaths::setRootOverride(std::filesystem::path root)
{
    const std::lock_guard lock(overrideMutex());
    overrideSlot() = std::move(root);
}

void AppPaths::clearRootOverride()
{
    const std::lock_guard lock(overrideMutex());
    overrideSlot().reset();
}

std::optional<std::filesystem::path> AppPaths::rootOverride()
{
    const std::lock_guard lock(overrideMutex());
    return overrideSlot();
}

std::filesystem::path AppPaths::portableRoot()
{
    return executableDir() / "portable";
}

#ifdef _WIN32
std::filesystem::path AppPaths::executableDir()
{
    constexpr std::size_t INITIAL_BUFFER_SIZE = MAX_PATH;
    constexpr std::size_t MAX_BUFFER_SIZE = 32768;

    std::vector<wchar_t> buffer(INITIAL_BUFFER_SIZE);
    while (buffer.size() <= MAX_BUFFER_SIZE)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            break;

        // A length equal to the buffer size means the path was truncated.
        if (length < buffer.size())
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();

        buffer.resize(buffer.size() * 2);
    }

    return std::filesystem::current_path();
}
#else
std::filesystem::path AppPaths::executableDir()
{
    std::error_code errorCode;
    const auto executablePath = std::filesystem::read_symlink("/proc/self/exe", errorCode);

    if (errorCode)
        return std::filesystem::current_path();

    return executablePath.parent_path();
}
#endif