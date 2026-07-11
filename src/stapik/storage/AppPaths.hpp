#pragma once

#include <filesystem>

class AppPaths
{
public:
    static std::filesystem::path resourcesDir();
    [[nodiscard]] static std::filesystem::path userDataDir(const std::string& appName);
private:
    static std::filesystem::path executableDir();
};