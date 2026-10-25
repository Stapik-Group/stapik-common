#pragma once

#include "stapik/storage/FileError.hpp"
#include "stapik/time/Clock.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>

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
        explicit SettingsStore(std::filesystem::path filePath, int schemaVersion = 1, const time::IClock& clock = time::systemClock());

        LoadStatus load();

        [[nodiscard]] storage::FileResult save() const;
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

        [[nodiscard]] std::string serialize() const;
        [[nodiscard]] bool replaceFromText(std::string_view text);
        storage::FileResult reset();
        [[nodiscard]] int schemaVersion() const;
        [[nodiscard]] const std::filesystem::path& filePath() const;

    private:
        static void logTypeMismatch(const std::string& key, const char* reason);
        [[nodiscard]] static bool hasSettingsStructure(const nlohmann::json& root);
        void adopt(const nlohmann::json& root, const std::string& origin);
        LoadStatus quarantineCorruptedFile(const char* reason) const;

        std::filesystem::path m_filePath;
        int m_schemaVersion;
        const time::IClock* m_clock;
        nlohmann::json m_values = nlohmann::json::object();
    };
}
