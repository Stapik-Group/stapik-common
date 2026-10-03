#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <map>

namespace stapik::document
{
    enum class MigrationStatus
    {
        UpToDate,
        Migrated,
        FromNewerVersion,
        MissingStep,
        StepFailed
    };

    class SchemaMigrator
    {
    public:
        using Step = std::function<void(nlohmann::json&)>;
        explicit SchemaMigrator(int currentVersion);
        SchemaMigrator& addStep(int fromVersion, Step step);

        [[nodiscard]] bool hasStep(int fromVersion) const;
        [[nodiscard]] int currentVersion() const;
        [[nodiscard]] MigrationStatus migrate(nlohmann::json& document, int fromVersion) const;
    private:
        int m_currentVersion;
        std::map<int, Step> m_steps;
    };
}
