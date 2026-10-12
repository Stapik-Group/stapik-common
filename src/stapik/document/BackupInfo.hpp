#pragma once

#include <chrono>
#include <filesystem>

namespace stapik::document
{
    struct BackupInfo
    {
        std::filesystem::path path;
        int index = 0;
        std::chrono::system_clock::time_point savedAt;
    };
}
