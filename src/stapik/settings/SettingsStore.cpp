#include "SettingsStore.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/storage/Quarantine.hpp"
#include "stapik/text/JsonText.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace stapik::settings
{
    SettingsStore::SettingsStore(std::filesystem::path filePath, const int schemaVersion, const time::IClock& clock) :
        m_filePath(std::move(filePath)),
        m_schemaVersion(schemaVersion),
        m_clock(&clock)
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

        if (!hasSettingsStructure(root))
            return quarantineCorruptedFile("unexpected structure");

        adopt(root, storage::pathText(m_filePath));
        return LoadStatus::Loaded;
    }

    storage::FileResult SettingsStore::save() const
    {
        return storage::writeFileAtomically(m_filePath, serialize());
    }

    std::string SettingsStore::serialize() const
    {
        const nlohmann::json root = {
            { "version", m_schemaVersion },
            { "settings", m_values }
        };

        return text::dumpJson(root, 2) + "\n";
    }

    bool SettingsStore::replaceFromText(const std::string_view text)
    {
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(text);
        }
        catch (const nlohmann::json::exception& exception)
        {
            log::warning("Settings to import are not valid JSON: {}", exception.what());
            return false;
        }

        if (!hasSettingsStructure(root))
        {
            log::warning("Settings to import have an unexpected structure");
            return false;
        }

        adopt(root, "imported text");
        return true;
    }

    bool SettingsStore::contains(const std::string& key) const
    {
        return m_values.contains(key);
    }

    void SettingsStore::remove(const std::string& key)
    {
        m_values.erase(key);
    }

    storage::FileResult SettingsStore::reset()
    {
        m_values = nlohmann::json::object();

        if (std::error_code errorCode; !std::filesystem::exists(m_filePath, errorCode))
            return {};

        auto backupPath = m_filePath;
        backupPath += ".bak";

        std::error_code errorCode;
        std::filesystem::rename(m_filePath, backupPath, errorCode);
        if (errorCode)
        {
            log::warning("Cannot move settings file {} aside: {}", storage::pathText(m_filePath), errorCode.message());
            return Failure(storage::FileError{ .path = m_filePath, .step = "move aside", .code = errorCode });
        }

        log::info("Settings reset; the previous file is kept as {}", storage::pathText(backupPath));
        return {};
    }

    int SettingsStore::schemaVersion() const
    {
        return m_schemaVersion;
    }

    const std::filesystem::path& SettingsStore::filePath() const
    {
        return m_filePath;
    }

    bool SettingsStore::hasSettingsStructure(const nlohmann::json& root)
    {
        return root.is_object() && root.contains("settings") && root.at("settings").is_object();
    }

    void SettingsStore::adopt(const nlohmann::json& root, const std::string& origin)
    {
        m_values = root.at("settings");

        if (root.contains("version") && root.at("version").is_number_integer())
        {
            if (const auto fileVersion = root.at("version").get<int>(); fileVersion > m_schemaVersion)
            {
                log::warning("Settings from {} have newer schema version {} (supported: {})", origin, fileVersion, m_schemaVersion);
                m_schemaVersion = fileVersion;
            }
        }
    }

    void SettingsStore::logTypeMismatch(const std::string& key, const char* reason)
    {
        log::warning("Setting '{}' has an unexpected type, using the default: {}", key, reason);
    }

    LoadStatus SettingsStore::quarantineCorruptedFile(const char* reason) const
    {
        const auto quarantined = storage::quarantineFile(m_filePath, m_clock->now());

        if (!quarantined)
        {
            log::warning("Settings file {} is corrupted ({}) and could not be moved aside: {}",
                storage::pathText(m_filePath), reason, quarantined.error().code.message());
        }
        else
        {
            log::warning("Settings file {} is corrupted ({}); moved to {}",
                storage::pathText(m_filePath), reason, storage::pathText(*quarantined));
        }

        return LoadStatus::Corrupted;
    }
}
