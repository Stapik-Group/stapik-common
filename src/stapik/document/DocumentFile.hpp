#pragma once

#include "DocumentFileCore.hpp"

#include "stapik/log/Log.hpp"

#include <nlohmann/json.hpp>

#include <concepts>
#include <exception>
#include <filesystem>
#include <optional>
#include <utility>

namespace stapik::document
{
    template<typename DocumentType>
    concept PersistableDocument = requires(const DocumentType& document, const nlohmann::json& json)
    {
        { document.toJson() } -> std::same_as<nlohmann::json>;
        { DocumentType::fromJson(json) } -> std::same_as<DocumentType>;
    };

    template<typename DocumentType>
    struct LoadResult
    {
        LoadStatus status = LoadStatus::Missing;
        std::optional<DocumentType> document;
        bool migrated = false;
        int fileVersion = 0;
    };

    template<PersistableDocument DocumentType>
    class DocumentFile : public DocumentFileCore
    {
    public:
        DocumentFile(std::filesystem::path filePath, const int currentSchemaVersion) :
            DocumentFileCore(std::move(filePath), SchemaMigrator(currentSchemaVersion))
        {}

        DocumentFile(std::filesystem::path filePath, SchemaMigrator migrator) :
            DocumentFileCore(std::move(filePath), std::move(migrator))
        {}

        [[nodiscard]] LoadResult<DocumentType> load() const
        {
            return convert(readRaw(filePath()), true);
        }

        [[nodiscard]] bool save(const DocumentType& document) const
        {
            return writeRaw(document.toJson());
        }

        [[nodiscard]] bool exportTo(const DocumentType& document, const std::filesystem::path& target) const
        {
            return writeRawTo(target, document.toJson());
        }

        [[nodiscard]] LoadResult<DocumentType> importFrom(const std::filesystem::path& source) const
        {
            return convert(readRaw(source), false);
        }

    private:
        LoadResult<DocumentType> convert(RawLoad raw, const bool quarantineIfCorrupted) const
        {
            LoadResult<DocumentType> result;
            result.status = raw.status;
            result.migrated = raw.migrated;
            result.fileVersion = raw.fileVersion;

            if (raw.status == LoadStatus::Loaded)
            {
                try
                {
                    result.document = DocumentType::fromJson(raw.document);
                }
                catch (const std::exception& exception)
                {
                    log::warning("Document content is invalid: {}", exception.what());
                    result.status = LoadStatus::Corrupted;
                }
            }

            if (result.status == LoadStatus::Corrupted && quarantineIfCorrupted)
                quarantineFile();

            return result;
        }
    };
}
