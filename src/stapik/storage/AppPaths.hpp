#pragma once

#include <filesystem>

class AppPaths
{
public:
    static std::filesystem::path resourcesDir();
private:
    static std::filesystem::path executableDir();
};