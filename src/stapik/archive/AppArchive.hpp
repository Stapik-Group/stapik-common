#pragma once

#include "ArchiveError.hpp"
#include "ZipReader.hpp"

#include "stapik/cloud/IAssetStorage.hpp"
#include "stapik/document/DocumentFile.hpp"
#include "stapik/settings/SettingsStore.hpp"
#include "stapik/storage/PathText.hpp"
#include "stapik/time/Clock.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stapik::archive
{
    inline constexpr int ARCHIVE_FORMAT_VERSION = 1;

    struct AppArchiveContext
    {
        std::string appName;
        std::string appVersion;
        settings::SettingsStore *settings = nullptr;
        const cloud::IAssetStorage *assets = nullptr;
    };

    struct ArchiveInfo
    {
        std::string appName;
        std::string appVersion;
        time::TimePoint createdAt;
        std::size_t assetCount = 0;
    };

    struct RestoreSummary
    {
        std::size_t assetsUploaded = 0;
        std::size_t assetsUnchanged = 0;
        std::size_t assetsNotRestored = 0;
        bool settingsRestored = false;
    };

    [[nodiscard]] ArchiveResult<ArchiveInfo> writeAppArchive(
        const std::filesystem::path &target,
        const AppArchiveContext &context,
        std::string_view documentText,
        const time::IClock &clock = time::systemClock());

    class AppArchive
    {
    public:
        [[nodiscard]] static ArchiveResult<AppArchive> open(std::filesystem::path source,
                                                            std::string_view expectedAppName);

        [[nodiscard]] const ArchiveInfo &info() const;

        [[nodiscard]] const std::string &documentText() const;

        [[nodiscard]] ArchiveResult<RestoreSummary> restoreAssets(const cloud::IAssetStorage &storage) const;

        [[nodiscard]] ArchiveResult<bool> restoreSettings(settings::SettingsStore &store) const;

    private:
        struct ArchivedAsset
        {
            std::string filename;
            std::string mimeType;
            std::int64_t sizeBytes = 0;
            std::string checksumSha256;
        };

        AppArchive(ZipReader zip, ArchiveInfo info, std::string documentText, std::optional<std::string> settingsText,
                   std::vector<ArchivedAsset> assets);

        ZipReader m_zip;
        ArchiveInfo m_info;
        std::string m_documentText;
        std::optional<std::string> m_settingsText;
        std::vector<ArchivedAsset> m_assets;
    };

    template<document::PersistableDocument DocumentType>
    struct RestoredApp
    {
        DocumentType document;
        ArchiveInfo info;
        RestoreSummary summary;
    };

    // "Back up the whole application" in one call.
    template<document::PersistableDocument DocumentType>
    [[nodiscard]] ArchiveResult<ArchiveInfo> exportAppArchive(
        const document::DocumentFile<DocumentType> &file,
        const DocumentType &document,
        const AppArchiveContext &context,
        const std::filesystem::path &target,
        const time::IClock &clock = time::systemClock())
    {
        return writeAppArchive(target, context, file.serialize(document), clock);
    }

    template<document::PersistableDocument DocumentType>
    [[nodiscard]] ArchiveResult<RestoredApp<DocumentType> > importAppArchive(
        const document::DocumentFile<DocumentType> &file,
        const std::filesystem::path &source,
        const AppArchiveContext &context)
    {
        auto archive = AppArchive::open(source, context.appName);
        if (!archive)
            return Failure(std::move(archive).error());

        auto loaded = file.deserialize(archive->documentText(), "the archive " + storage::pathText(source));
        switch (loaded.status)
        {
            using enum document::LoadStatus;
            case Loaded:
                break;
            case NewerVersion:
                return archiveFailure(ArchiveErrorKind::Unsupported,
                                      "The archive was made by a newer version of the application");
            case Missing:
            case Corrupted:
            case MigrationFailed:
            default:
                return archiveFailure(ArchiveErrorKind::Corrupted, "The document in the archive cannot be read");
        }

        RestoreSummary summary;
        if (context.assets != nullptr)
        {
            auto restored = archive->restoreAssets(*context.assets);
            if (!restored)
                return Failure(std::move(restored).error());
            summary = *restored;
        } else
        {
            summary.assetsNotRestored = archive->info().assetCount;
        }

        if (const auto saved = file.save(*loaded.document); !saved)
            return archiveFailure(ArchiveErrorKind::Io, saved.error().message());

        if (context.settings != nullptr)
        {
            // The document is already restored here, so a failure is reported through the summary, not as an error.
            const auto restored = archive->restoreSettings(*context.settings);
            summary.settingsRestored = restored && *restored;
        }

        return RestoredApp<DocumentType>{std::move(*loaded.document), archive->info(), summary};
    }
}
