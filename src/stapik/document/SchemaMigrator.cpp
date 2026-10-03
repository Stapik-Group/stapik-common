#include "SchemaMigrator.hpp"

#include "stapik/log/Log.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace stapik::document
{
    SchemaMigrator::SchemaMigrator(const int currentVersion) :
        m_currentVersion(currentVersion)
    {}

    SchemaMigrator& SchemaMigrator::addStep(const int fromVersion, Step step)
    {
        if (fromVersion < 0 || fromVersion >= m_currentVersion)
            throw std::invalid_argument("Migration step version must be in [0, currentVersion)");

        m_steps[fromVersion] = std::move(step);
        return *this;
    }

    bool SchemaMigrator::hasStep(const int fromVersion) const
    {
        return m_steps.contains(fromVersion);
    }

    int SchemaMigrator::currentVersion() const
    {
        return m_currentVersion;
    }

    MigrationStatus SchemaMigrator::migrate(nlohmann::json& document, const int fromVersion) const
    {
        if (fromVersion > m_currentVersion)
            return MigrationStatus::FromNewerVersion;

        if (fromVersion == m_currentVersion)
            return MigrationStatus::UpToDate;

        auto working = document;

        for (int version = fromVersion; version < m_currentVersion; ++version)
        {
            const auto step = m_steps.find(version);
            if (step == m_steps.end())
            {
                log::warning("No migration step from schema version {} to {}", version, version + 1);
                return MigrationStatus::MissingStep;
            }

            try
            {
                step->second(working);
            }
            catch (const std::exception& exception)
            {
                log::warning("Migration from schema version {} to {} failed: {}", version, version + 1, exception.what());
                return MigrationStatus::StepFailed;
            }
        }

        document = std::move(working);
        return MigrationStatus::Migrated;
    }
}
