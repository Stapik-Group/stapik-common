#include "AppArchive.hpp"

#include "ZipWriter.hpp"

#include "stapik/cloud/CloudAssetProtocol.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/sync/Timestamp.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>

namespace stapik::archive
{
    namespace
    {
        constexpr std::string_view FORMAT_NAME = "stapik-archive";
        constexpr std::string_view MANIFEST_ENTRY = "manifest.json";
        constexpr std::string_view DOCUMENT_ENTRY = "document.json";
        constexpr std::string_view SETTINGS_ENTRY = "settings.json";
        constexpr std::string_view ASSETS_FOLDER = "assets/";
        constexpr std::string_view DEFAULT_MIME_TYPE = "application/octet-stream";

        // A filename becomes a name inside the archive, so it must not be able to leave the "assets" folder.
        bool isSafeAssetFilename(const std::string_view filename)
        {
            return !filename.empty()
                && filename != "." && filename != ".."
                && std::ranges::none_of(filename, [](const char character)
                {
                    return character == '/' || character == '\\' || static_cast<unsigned char>(character) < 0x20;
                });
        }

        std::string assetEntryName(const std::string_view filename)
        {
            return std::string(ASSETS_FOLDER) + std::string(filename);
        }

        bool sameChecksum(const std::string_view first, const std::string_view second)
        {
            return std::ranges::equal(first, second, [](const char left, const char right)
            {
                return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
            });
        }

        ArchiveError cloudError(const std::string& detail)
        {
            return ArchiveError{ .kind = ArchiveErrorKind::Cloud, .detail = detail };
        }
    }

    ArchiveResult<ArchiveInfo> writeAppArchive(
        const std::filesystem::path& target,
        const AppArchiveContext& context,
        const std::string_view documentText,
        const time::IClock& clock)
    {
        std::vector<cloud::AssetInfo> assets;
        if (context.assets != nullptr)
        {
            try
            {
                assets = context.assets->listAssets();
            }
            catch (const CloudStorageException& exception)
            {
                return Failure(cloudError(std::string("Cannot list the files in the cloud: ") + exception.what()));
            }
        }

        for (const auto& asset : assets)
        {
            if (!isSafeAssetFilename(asset.filename))
                return archiveFailure(ArchiveErrorKind::Unsupported, "The file name \"" + asset.filename + "\" cannot be stored in an archive");
        }

        const auto createdAt = clock.now();
        auto writer = ZipWriter::create(target, createdAt);
        if (!writer)
            return Failure(std::move(writer).error());

        if (auto added = (*writer)->addText(DOCUMENT_ENTRY, documentText); !added)
            return Failure(std::move(added).error());

        if (context.settings != nullptr)
        {
            if (auto added = (*writer)->addText(SETTINGS_ENTRY, context.settings->serialize()); !added)
                return Failure(std::move(added).error());
        }

        nlohmann::json manifestAssets = nlohmann::json::array();
        for (const auto& asset : assets)
        {
            std::optional<cloud::AssetBytes> content;
            try
            {
                content = context.assets->downloadAsset(asset.filename);
            }
            catch (const CloudStorageException& exception)
            {
                return Failure(cloudError("Cannot download \"" + asset.filename + "\": " + exception.what()));
            }

            if (!content)
                return Failure(cloudError("\"" + asset.filename + "\" disappeared from the cloud during the backup"));

            const auto checksum = cloud::sha256Hex(*content);
            if (!asset.checksumSha256.empty() && !sameChecksum(checksum, asset.checksumSha256))
                return Failure(cloudError("\"" + asset.filename + "\" was damaged while downloading"));

            if (auto added = (*writer)->add(assetEntryName(asset.filename), *content); !added)
                return Failure(std::move(added).error());

            manifestAssets.push_back({
                { "filename", asset.filename },
                { "mimeType", asset.mimeType },
                { "sizeBytes", content->size() },
                { "checksumSha256", checksum } });
        }

        const nlohmann::json manifest = {
            { "format", std::string(FORMAT_NAME) },
            { "formatVersion", ARCHIVE_FORMAT_VERSION },
            { "app", context.appName },
            { "appVersion", context.appVersion },
            { "createdAt", sync::toIso8601(createdAt) },
            { "assets", manifestAssets }
        };

        if (auto added = (*writer)->addText(MANIFEST_ENTRY, manifest.dump(2) + "\n"); !added)
            return Failure(std::move(added).error());

        if (auto finished = (*writer)->finish(); !finished)
            return Failure(std::move(finished).error());

        log::info("Application archive written to {} ({} assets)", storage::pathText(target), assets.size());

        return ArchiveInfo{
            .appName = context.appName,
            .appVersion = context.appVersion,
            .createdAt = createdAt,
            .assetCount = assets.size() };
    }

    AppArchive::AppArchive(ZipReader zip, ArchiveInfo info, std::string documentText, std::optional<std::string> settingsText, std::vector<ArchivedAsset> assets) :
        m_zip(std::move(zip)),
        m_info(std::move(info)),
        m_documentText(std::move(documentText)),
        m_settingsText(std::move(settingsText)),
        m_assets(std::move(assets))
    {}

    ArchiveResult<AppArchive> AppArchive::open(std::filesystem::path source, const std::string_view expectedAppName)
    {
        auto zip = ZipReader::open(source);
        if (!zip)
            return Failure(std::move(zip).error());

        const auto notStapik = [&source]
        {
            return archiveFailure(ArchiveErrorKind::NotAnArchive, storage::pathText(source) + " is not an application archive");
        };

        if (zip->find(MANIFEST_ENTRY) == nullptr)
            return notStapik();

        auto manifestText = zip->readText(MANIFEST_ENTRY);
        if (!manifestText)
            return Failure(std::move(manifestText).error());

        ArchiveInfo info;
        std::vector<ArchivedAsset> assets;
        try
        {
            const auto manifest = nlohmann::json::parse(*manifestText);

            if (manifest.value("format", "") != FORMAT_NAME)
                return notStapik();

            if (manifest.at("formatVersion").get<int>() > ARCHIVE_FORMAT_VERSION)
                return archiveFailure(ArchiveErrorKind::Unsupported, "The archive was made by a newer version of the application");

            info.appName = manifest.at("app").get<std::string>();
            info.appVersion = manifest.at("appVersion").get<std::string>();

            const auto createdAt = sync::parseIso8601(manifest.at("createdAt").get<std::string>());
            if (!createdAt)
                return archiveFailure(ArchiveErrorKind::Corrupted, "The archive has an invalid date");
            info.createdAt = *createdAt;

            for (const auto& item : manifest.at("assets"))
            {
                ArchivedAsset asset{
                    .filename = item.at("filename").get<std::string>(),
                    .mimeType = item.at("mimeType").get<std::string>(),
                    .sizeBytes = item.at("sizeBytes").get<std::int64_t>(),
                    .checksumSha256 = item.at("checksumSha256").get<std::string>() };

                if (!isSafeAssetFilename(asset.filename))
                    return archiveFailure(ArchiveErrorKind::Corrupted, "The archive lists an invalid file name");

                assets.push_back(std::move(asset));
            }
        }
        catch (const nlohmann::json::exception& exception)
        {
            return archiveFailure(ArchiveErrorKind::Corrupted, std::string("The manifest of the archive is damaged: ") + exception.what());
        }

        if (info.appName != expectedAppName)
            return archiveFailure(ArchiveErrorKind::WrongApplication, "The archive belongs to \"" + info.appName + "\", not to \"" + std::string(expectedAppName) + "\"");

        info.assetCount = assets.size();

        auto documentText = zip->readText(DOCUMENT_ENTRY);
        if (!documentText)
            return Failure(std::move(documentText).error());

        std::optional<std::string> settingsText;
        if (zip->find(SETTINGS_ENTRY) != nullptr)
        {
            auto text = zip->readText(SETTINGS_ENTRY);
            if (!text)
                return Failure(std::move(text).error());

            // Tried on a store that is not connected to any file, only to be sure that the text can be applied later.
            if (settings::SettingsStore scratch({}); !scratch.replaceFromText(*text))
                return archiveFailure(ArchiveErrorKind::Corrupted, "The settings in the archive are damaged");

            settingsText = std::move(*text);
        }

        for (const auto& asset : assets)
        {
            const auto content = zip->read(assetEntryName(asset.filename));
            if (!content)
                return Failure(content.error());

            if (static_cast<std::int64_t>(content->size()) != asset.sizeBytes || !sameChecksum(cloud::sha256Hex(*content), asset.checksumSha256))
                return archiveFailure(ArchiveErrorKind::Corrupted, "\"" + asset.filename + "\" does not match the manifest of the archive");
        }

        return AppArchive(std::move(*zip), std::move(info), std::move(*documentText), std::move(settingsText), std::move(assets));
    }

    const ArchiveInfo& AppArchive::info() const
    {
        return m_info;
    }

    const std::string& AppArchive::documentText() const
    {
        return m_documentText;
    }

    ArchiveResult<RestoreSummary> AppArchive::restoreAssets(const cloud::IAssetStorage& storage) const
    {
        std::map<std::string, std::string> existing;
        try
        {
            for (const auto& asset : storage.listAssets())
                existing[asset.filename] = asset.checksumSha256;
        }
        catch (const CloudStorageException& exception)
        {
            return Failure(cloudError(std::string("Cannot list the files in the cloud: ") + exception.what()));
        }

        RestoreSummary summary;
        for (const auto& asset : m_assets)
        {
            if (const auto found = existing.find(asset.filename); found != existing.end() && sameChecksum(found->second, asset.checksumSha256))
            {
                ++summary.assetsUnchanged;
                continue;
            }

            const auto content = m_zip.read(assetEntryName(asset.filename));
            if (!content)
                return Failure(content.error());

            try
            {
                static_cast<void>(storage.uploadAsset(asset.filename, asset.mimeType.empty() ? std::string(DEFAULT_MIME_TYPE) : asset.mimeType, *content));
            }
            catch (const cloud::AssetTooLargeException&)
            {
                return Failure(cloudError("\"" + asset.filename + "\" is too big for the cloud"));
            }
            catch (const CloudStorageException& exception)
            {
                return Failure(cloudError("Cannot upload \"" + asset.filename + "\": " + exception.what()));
            }

            ++summary.assetsUploaded;
        }

        return summary;
    }

    ArchiveResult<bool> AppArchive::restoreSettings(settings::SettingsStore& store) const
    {
        if (!m_settingsText)
            return false;

        if (!store.replaceFromText(*m_settingsText))
            return archiveFailure(ArchiveErrorKind::Corrupted, "The settings in the archive are damaged");

        if (const auto saved = store.save(); !saved)
        {
            log::warning("Cannot save the restored settings: {}", saved.error().message());
            return archiveFailure(ArchiveErrorKind::Io, saved.error().message());
        }

        return true;
    }
}
