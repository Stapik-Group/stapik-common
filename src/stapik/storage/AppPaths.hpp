#pragma once

#include <filesystem>

class AppPaths
{
public:
    static std::filesystem::path resourcesDir();
    static std::filesystem::path ensureUserDataDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path userDataDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path userConfigDir(const std::string& appName);
    [[nodiscard]] static std::filesystem::path userCacheDir(const std::string& appName);
private:
    static std::filesystem::path executableDir();
};