#pragma once

#include "SchemaMigrator.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace stapik::document
{
    enum class LoadStatus
    {
        Loaded,
        Missing,
        Corrupted,
        NewerVersion,
        MigrationFailed
    };

    struct RawLoad
    {
        LoadStatus status = LoadStatus::Missing;
        nlohmann::json document;
        int fileVersion = 0;
        bool migrated = false;
    };

    class DocumentFileCore
    {
    public:
        DocumentFileCore(std::filesystem::path filePath, SchemaMigrator migrator);
        void setBackupCount(int backupCount);
        [[nodiscard]] const std::filesystem::path& filePath() const;
        [[nodiscard]] const SchemaMigrator& migrator() const;

    protected:
        void quarantineFile() const;
        [[nodiscard]] RawLoad readRaw(const std::filesystem::path& source) const;
        [[nodiscard]] bool writeRaw(const nlohmann::json& document) const;
        [[nodiscard]] bool writeRawTo(const std::filesystem::path& target, const nlohmann::json& document) const;
    private:
        [[nodiscard]] std::filesystem::path backupPath(int index) const;
        void rotateBackups() const;

        std::filesystem::path m_filePath;
        SchemaMigrator m_migrator;
        int m_backupCount = 1;
    };
}
