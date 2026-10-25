#include "DocumentFileCore.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/storage/AtomicFile.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/text/JsonText.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
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

            return nlohmann::json::accept(file);
        }

        std::chrono::system_clock::time_point toSystemTime(const std::filesystem::file_time_type fileTime)
        {
            return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                std::chrono::clock_cast<std::chrono::system_clock>(fileTime));
        }
    }

    DocumentFileCore::DocumentFileCore(std::filesystem::path filePath, SchemaMigrator migrator,
                                       const time::IClock& clock) :
        m_filePath(std::move(filePath)),
        m_migrator(std::move(migrator)),
        m_clock(&clock)
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

    std::vector<BackupInfo> DocumentFileCore::listBackups() const
    {
        std::vector<BackupInfo> backups;

        for (int index = 1; index <= m_backupCount; ++index)
        {
            const auto path = backupPath(index);

            std::error_code errorCode;
            if (!std::filesystem::is_regular_file(path, errorCode))
                continue;

            const auto savedAt = std::filesystem::last_write_time(path, errorCode);
            if (errorCode)
                continue;

            backups.push_back({ .path = path, .index = index, .savedAt = toSystemTime(savedAt) });
        }

        return backups;
    }

    RawLoad DocumentFileCore::readRaw(const std::filesystem::path& source) const
    {
        RawLoad result;

        if (std::error_code errorCode; !std::filesystem::exists(source, errorCode))
            return result;

        std::ifstream file(source);
        if (!file.is_open())
        {
            log::warning("Cannot open document file {}", storage::pathText(source));
            return result;
        }

        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        return parseEnvelope(text, storage::pathText(source));
    }

    RawLoad DocumentFileCore::parseEnvelope(const std::string_view text, const std::string_view origin) const
    {
        RawLoad result;

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(text);
        }
        catch (const nlohmann::json::exception& exception)
        {
            log::warning("Document file {} is not valid JSON: {}", origin, exception.what());
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
                log::warning("Document file {} has schema version {}, newer than supported {}", origin, fileVersion, m_migrator.currentVersion());
                result.status = NewerVersion;
                break;
            case MissingStep:
            case StepFailed:
                result.status = MigrationFailed;
                break;
        }

        return result;
    }

    storage::FileResult DocumentFileCore::writeRaw(const nlohmann::json& document) const
    {
        rotateBackups();
        return writeRawTo(m_filePath, document);
    }

    storage::FileResult DocumentFileCore::writeRawTo(const std::filesystem::path& target, const nlohmann::json& document) const
    {
        return storage::writeFileAtomically(target, envelopeText(document));
    }

    std::string DocumentFileCore::envelopeText(const nlohmann::json& document) const
    {
        const nlohmann::json root = {
            { "schemaVersion", m_migrator.currentVersion() },
            { "document", document }
        };

        return text::dumpJson(root, 2) + "\n";
    }

    void DocumentFileCore::quarantineFile() const
    {
        const auto stamp = std::format(
            "{:%Y%m%d-%H%M%S}",
            std::chrono::floor<std::chrono::seconds>(m_clock->now()));

        std::error_code errorCode;
        auto target = m_filePath;
        target += std::format(".corrupt-{}", stamp);

        int suffix = 2;
        while (std::filesystem::exists(target, errorCode))
        {
            target = m_filePath;
            target += std::format(".corrupt-{}-{}", stamp, suffix++);
        }

        std::filesystem::rename(m_filePath, target, errorCode);

        if (errorCode)
            log::warning("Cannot move corrupted document {} aside: {}", storage::pathText(m_filePath), errorCode.message());
        else
            log::warning("Corrupted document moved to {}", storage::pathText(target));
    }

    std::filesystem::path DocumentFileCore::backupPath(const int index) const
    {
        auto backup = m_filePath;
        backup += index == 1 ? std::string(".bak") : std::format(".bak.{}", index);
        return backup;
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
        {
            log::warning("Cannot back up {}: {}", storage::pathText(m_filePath), errorCode.message());
            return;
        }

        // Copying sets the date on Linux but keeps it on Windows; the backups are listed by the date of their contents.
        const auto savedAt = std::filesystem::last_write_time(m_filePath, errorCode);
        if (!errorCode)
            std::filesystem::last_write_time(backupPath(1), savedAt, errorCode);

        if (errorCode)
            log::warning("Cannot set the date of the backup of {}: {}", storage::pathText(m_filePath), errorCode.message());
    }
}
