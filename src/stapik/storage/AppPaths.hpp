#pragma once

#include <filesystem>
#include <string>

class AppPaths
{
public:
    static std::filesystem::path resourcesDir();

    [[nodiscard]] static std::filesystem::path commonResourcesDir();
    [[nodiscard]] static std::filesystem::path resolveCommonResourcesDir(
        const std::filesystem::path& executableDir,
        const std::filesystem::path& sourceTreeDir);
    static std::filesystem::path ensureUserDataDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path userDataDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path userConfigDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path userCacheDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path executableDir();
};