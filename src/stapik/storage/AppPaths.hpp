#pragma once

#include <filesystem>
#include <optional>
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

    // Moves data, config and cache under <root>/{data,config,cache}, ignoring the platform defaults and XDG_*_HOME.
    // Must be called before anything reads settings or user data: stores created earlier keep their old paths.
    static void setRootOverride(std::filesystem::path root);
    static void clearRootOverride();
    [[nodiscard]] static std::optional<std::filesystem::path> rootOverride();
    [[nodiscard]] static std::filesystem::path portableRoot();
};