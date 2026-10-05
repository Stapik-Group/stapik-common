#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace stapik::app
{
    [[nodiscard]] std::string rebasePixbufLoadersCache(std::string_view cacheText, const std::filesystem::path& loadersDirectory);
    void configureBundledRuntime(const std::filesystem::path& bundleRoot, const std::filesystem::path& cacheDirectory);
    void prepareWindowsRuntime(const std::string& appName);
}
