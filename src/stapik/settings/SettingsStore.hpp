#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace stapik::settings
{
    enum class LoadStatus
    {
        Loaded,
        Missing,
        Corrupted
    };

    class SettingsStore
    {
    public:
        explicit SettingsStore(std::filesystem::path filePath, int schemaVersion = 1);

        LoadStatus load();

        [[nodiscard]] bool save() const;
        [[nodiscard]] bool contains(const std::string& key) const;

        template<typename ValueType>
        [[nodiscard]] ValueType get(const std::string& key, const ValueType& defaultValue) const
        {
            const auto entry = m_values.find(key);
            if (entry == m_values.end())
                return defaultValue;

            try
            {
                return entry->get<ValueType>();
            }
            catch (const nlohmann::json::exception& exception)
            {
                logTypeMismatch(key, exception.what());
                return defaultValue;
            }
        }

        template<typename ValueType>
        void set(const std::string& key, const ValueType& value)
        {
            m_values[key] = value;
        }

        void remove(const std::string& key);

        [[nodiscard]] int schemaVersion() const;
        [[nodiscard]] const std::filesystem::path& filePath() const;

    private:
        static void logTypeMismatch(const std::string& key, const char* reason);
        LoadStatus quarantineCorruptedFile(const char* reason) const;

        std::filesystem::path m_filePath;
        int m_schemaVersion;
        nlohmann::json m_values = nlohmann::json::object();
    };
}
