#include "AppPaths.hpp"
#include <unistd.h>
#include <climits>

std::filesystem::path AppPaths::resourcesDir()
{
    if (const auto siblingResources = executableDir() / "resources"; std::filesystem::exists(siblingResources))
        return siblingResources;

    return executableDir().parent_path() / "resources";
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