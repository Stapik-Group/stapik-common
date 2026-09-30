#include "AppPaths.hpp"

#include "stapik/log/Log.hpp"

#include <climits>

namespace
{
    std::filesystem::path xdgAppDir(const char* xdgVariable, const char* homeRelativeDir, const std::string& appName)
    {
        if (const auto* xdgValue = std::getenv(xdgVariable); xdgValue != nullptr && *xdgValue != '\0')
            return std::filesystem::path(xdgValue) / appName;

        const auto* home = std::getenv("HOME");
        if (home == nullptr || *home == '\0')
            return std::filesystem::temp_directory_path() / appName;

        return std::filesystem::path(home) / homeRelativeDir / appName;
    }
}

std::filesystem::path AppPaths::resourcesDir()
{
    if (const auto siblingResources = executableDir() / "resources"; std::filesystem::exists(siblingResources))
        return siblingResources;

    return executableDir().parent_path() / "resources";
}

std::filesystem::path AppPaths::userDataDir(const std::string& appName)
{
    return xdgAppDir("XDG_DATA_HOME", ".local/share", appName);
}

std::filesystem::path AppPaths::userConfigDir(const std::string& appName)
{
    return xdgAppDir("XDG_CONFIG_HOME", ".config", appName);
}

std::filesystem::path AppPaths::userCacheDir(const std::string& appName)
{
    return xdgAppDir("XDG_CACHE_HOME", ".cache", appName);
}

std::filesystem::path AppPaths::ensureUserDataDir(const std::string& appName)
{
    const auto directory = userDataDir(appName);
    std::error_code errorCode;
    std::filesystem::create_directories(directory, errorCode);
    if (errorCode)
        stapik::log::warning("Cannot create user data directory {}: {}", directory.string(), errorCode.message());

    return directory;
}

std::filesystem::path AppPaths::executableDir()
{
    std::error_code errorCode;
    const auto executablePath = std::filesystem::read_symlink("/proc/self/exe", errorCode);

    if (errorCode)
        return std::filesystem::current_path();

    return executablePath.parent_path();
}