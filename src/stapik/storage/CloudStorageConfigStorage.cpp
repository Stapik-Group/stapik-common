#include "CloudStorageConfigStorage.hpp"
#include "stapik/storage/AppPaths.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

void CloudStorageConfigStorage::save(const CloudStorageConfig& config, const std::string& appName)
{
    const auto path = AppPaths::userDataDir(appName) / "config.json";
    std::filesystem::create_directories(path.parent_path());

    const nlohmann::json json = {
        { "apiUrl", config.apiUrl },
        { "apiKey", config.apiKey }
    };

    std::ofstream file(path);
    file << json.dump(2);
}

std::optional<CloudStorageConfig> CloudStorageConfigStorage::load(const std::string& appName)
{
    const auto path = AppPaths::userDataDir(appName) / "config.json";
    if (!std::filesystem::exists(path))
        return std::nullopt;

    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    try
    {
        const auto json = nlohmann::json::parse(file);

        return CloudStorageConfig{
            json.at("apiUrl").get<std::string>(),
            json.at("apiKey").get<std::string>()
        };
    }
    catch (const nlohmann::json::exception&)
    {
        return std::nullopt;
    }
}