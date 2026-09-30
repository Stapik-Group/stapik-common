#include "DocumentFileCore.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AtomicFile.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <utility>

namespace stapik::document
{
    namespace
    {
        bool isValidJsonFile(const std::filesystem::path& path)
        {
            std::ifstream file(path);
            if (!file.is_open())
                return false;

            try
            {
                static_cast<void>(nlohmann::json::parse(file));
                return true;
            }
            catch (const nlohmann::json::exception&)
            {
                return false;
            }
        }
    }

    DocumentFileCore::DocumentFileCore(std::filesystem::path filePath, SchemaMigrator migrator) :
        m_filePath(std::move(filePath)),
        m_migrator(std::move(migrator))
    {}

    void DocumentFileCore::setBackupCount(const int backupCount)
    {
        m_backupCount = std::max(backupCount, 0);
    }

    const std::filesystem::path& DocumentFileCore::filePath() const
    {
        return m_filePath;
    }

    const SchemaMigrator& DocumentFileCore::migrator() const
    {
        return m_migrator;
    }

    RawLoad DocumentFileCore::readRaw(const std::filesystem::path& source) const
    {
        RawLoad result;

        if (std::error_code errorCode; !std::filesystem::exists(source, errorCode))
            return result;

        std::ifstream file(source);
        if (!file.is_open())
        {
            log::warning("Cannot open document file {}", source.string());
            return result;
        }

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(file);
        }
        catch (const nlohmann::json::exception& exception)
        {
            log::warning("Document file {} is not valid JSON: {}", source.string(), exception.what());
            result.status = LoadStatus::Corrupted;
            return result;
        }

        const bool hasEnvelope = root.is_object()
            && root.contains("schemaVersion")
            && root.at("schemaVersion").is_number_integer()
            && root.contains("document");

        int fileVersion = 0;
        if (hasEnvelope)
        {
            fileVersion = root.at("schemaVersion").get<int>();
            result.document = root.at("document");
        }
        else
        {
            fileVersion = m_migrator.hasStep(0) ? 0 : m_migrator.currentVersion();
            result.document = std::move(root);
        }

        result.fileVersion = fileVersion;

        switch (m_migrator.migrate(result.document, fileVersion))
        {
            using enum MigrationStatus;
            using enum LoadStatus;
            case UpToDate:
                result.status = Loaded;
                break;
            case Migrated:
                result.status = Loaded;
                result.migrated = true;
                break;
            case FromNewerVersion:
                log::warning("Document file {} has schema version {}, newer than supported {}", source.string(), fileVersion, m_migrator.currentVersion());
                result.status = NewerVersion;
                break;
            case MissingStep:
            case StepFailed:
                result.status = MigrationFailed;
                break;
        }

        return result;
    }

    bool DocumentFileCore::writeRaw(const nlohmann::json& document) const
    {
        rotateBackups();
        return writeRawTo(m_filePath, document);
    }

    bool DocumentFileCore::writeRawTo(const std::filesystem::path& target, const nlohmann::json& document) const
    {
        const nlohmann::json root = {
            { "schemaVersion", m_migrator.currentVersion() },
            { "document", document }
        };

        return storage::writeFileAtomically(target, root.dump(2) + "\n");
    }

    void DocumentFileCore::quarantineFile() const
    {
        const auto stamp = std::format(
            "{:%Y%m%d-%H%M%S}",
            std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));

        std::error_code errorCode;
        auto target = std::filesystem::path{
            std::format("{}.corrupt-{}", m_filePath.string(), stamp)
        };

        int suffix = 2;
        while (std::filesystem::exists(target, errorCode))
        {
            target = std::filesystem::path{
                std::format("{}.corrupt-{}-{}", m_filePath.string(), stamp, suffix++)
            };
        }

        std::filesystem::rename(m_filePath, target, errorCode);

        if (errorCode)
            log::warning("Cannot move corrupted document {} aside: {}", m_filePath.string(), errorCode.message());
        else
            log::warning("Corrupted document moved to {}", target.string());
    }

    std::filesystem::path DocumentFileCore::backupPath(const int index) const
    {
        return index == 1
            ? std::format("{}.bak", m_filePath.string())
            : std::format("{}.bak.{}", m_filePath.string(), index);
    }

    void DocumentFileCore::rotateBackups() const
    {
        if (m_backupCount <= 0)
            return;

        std::error_code errorCode;
        if (!std::filesystem::exists(m_filePath, errorCode))
            return;

        if (!isValidJsonFile(m_filePath))
            return;

        for (int index = m_backupCount - 1; index >= 1; --index)
        {
            if (std::filesystem::exists(backupPath(index), errorCode))
                std::filesystem::rename(backupPath(index), backupPath(index + 1), errorCode);
        }

        std::filesystem::copy_file(m_filePath, backupPath(1), std::filesystem::copy_options::overwrite_existing, errorCode);

        if (errorCode)
            log::warning("Cannot back up {}: {}", m_filePath.string(), errorCode.message());
    }
}
