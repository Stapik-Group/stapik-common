#include "AppPaths.hpp"

#include <unistd.h>
#include <climits>

std::filesystem::path AppPaths::resourcesDir()
{
    if (const auto siblingResources = executableDir() / "resources"; std::filesystem::exists(siblingResources))
        return siblingResources;

    return executableDir().parent_path() / "resources";
}

std::filesystem::path AppPaths::userDataDir(const std::string& appName)
{
    if (const auto* xdgDataHome = std::getenv("XDG_DATA_HOME"))
        return std::filesystem::path(xdgDataHome) / appName;

    const auto* home = std::getenv("HOME");
    if (home == nullptr)
        return std::filesystem::temp_directory_path() / appName;

    return std::filesystem::path(home) / ".local" / "share" / appName;
}

std::filesystem::path AppPaths::executableDir()
{
    char buffer[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

    if (len == -1)
        return std::filesystem::current_path();

    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}