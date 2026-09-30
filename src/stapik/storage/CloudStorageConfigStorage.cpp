#include "CloudStorageConfigStorage.hpp"

#include "stapik/storage/AppPaths.hpp"
#include "stapik/log/Log.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <glib.h>

namespace
{
    void restrictToOwner(const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::permissions(
            path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace,
            errorCode);

        if (errorCode)
            stapik::log::warning("Cannot restrict permissions of {}: {}", path.string(), errorCode.message());
    }
}

void CloudStorageConfigStorage::save(const CloudStorageConfig& config, const std::string& appName)
{
    const auto path = AppPaths::userDataDir(appName) / "config.json";
    std::filesystem::create_directories(path.parent_path());

    const nlohmann::json json = {
        { "apiUrl", config.apiUrl },
        { "apiKey", config.apiKey }
    };

    std::ofstream file(path, std::ios::trunc);
    restrictToOwner(path);
    file << json.dump(2);
}

std::optional<CloudStorageConfig> CloudStorageConfigStorage::load(const std::string& appName)
{
    const auto path = AppPaths::userDataDir(appName) / "config.json";
    if (!std::filesystem::exists(path))
        return std::nullopt;

    restrictToOwner(path);

    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    try
    {
        const auto json = nlohmann::json::parse(file);

        return CloudStorageConfig{
            .apiUrl = json.at("apiUrl").get<std::string>(),
            .apiKey = json.at("apiKey").get<std::string>()
        };
    }
    catch (const nlohmann::json::exception&)
    {
        return std::nullopt;
    }
}