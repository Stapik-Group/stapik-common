#include "SettingsStore.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/storage/PathText.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace stapik::settings
{
    SettingsStore::SettingsStore(std::filesystem::path filePath, const int schemaVersion) :
        m_filePath(std::move(filePath)),
        m_schemaVersion(schemaVersion)
    {}

    LoadStatus SettingsStore::load()
    {
        m_values = nlohmann::json::object();

        if (std::error_code errorCode; !std::filesystem::exists(m_filePath, errorCode))
            return LoadStatus::Missing;

        std::ifstream file(m_filePath);
        if (!file.is_open())
        {
            log::warning("Cannot open settings file {}", storage::pathText(m_filePath));
            return LoadStatus::Missing;
        }

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(file);
        }
        catch (const nlohmann::json::exception& exception)
        {
            return quarantineCorruptedFile(exception.what());
        }

        if (!root.is_object() || !root.contains("settings") || !root.at("settings").is_object())
            return quarantineCorruptedFile("unexpected structure");

        m_values = root.at("settings");

        if (root.contains("version") && root.at("version").is_number_integer())
        {
            if (const auto fileVersion = root.at("version").get<int>(); fileVersion > m_schemaVersion)
            {
                log::warning("Settings file {} has newer schema version {} (supported: {})", storage::pathText(m_filePath), fileVersion, m_schemaVersion);
                m_schemaVersion = fileVersion;
            }
        }

        return LoadStatus::Loaded;
    }

    bool SettingsStore::save() const
    {
        const nlohmann::json root = {
            { "version", m_schemaVersion },
            { "settings", m_values }
        };

        return storage::writeFileAtomically(m_filePath, root.dump(2) + "\n");
    }

    bool SettingsStore::contains(const std::string& key) const
    {
        return m_values.contains(key);
    }

    void SettingsStore::remove(const std::string& key)
    {
        m_values.erase(key);
    }

    int SettingsStore::schemaVersion() const
    {
        return m_schemaVersion;
    }

    const std::filesystem::path& SettingsStore::filePath() const
    {
        return m_filePath;
    }

    void SettingsStore::logTypeMismatch(const std::string& key, const char* reason)
    {
        log::warning("Setting '{}' has an unexpected type, using the default: {}", key, reason);
    }

    LoadStatus SettingsStore::quarantineCorruptedFile(const char* reason) const
    {
        auto quarantinePath = m_filePath;
        quarantinePath += ".corrupt";

        std::error_code errorCode;
        std::filesystem::rename(m_filePath, quarantinePath, errorCode);

        if (errorCode)
        {
            log::warning("Settings file {} is corrupted ({}) and could not be moved aside: {}",
                storage::pathText(m_filePath), reason, errorCode.message());
        }
        else
        {
            log::warning("Settings file {} is corrupted ({}); moved to {}",
                storage::pathText(m_filePath), reason, storage::pathText(quarantinePath));
        }

        return LoadStatus::Corrupted;
    }
}
