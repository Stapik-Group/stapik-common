#include "stapik/archive/AppArchive.hpp"
#include "stapik/archive/ZipWriter.hpp"

#include "support/FakeAssetStorage.hpp"
#include "support/FakeClock.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace stapik::archive;
    using stapik::document::DocumentFile;
    using stapik::settings::SettingsStore;
    using stapik::test::FakeAssetStorage;

    struct Note
    {
        std::string title;
        int revision = 0;

        [[nodiscard]] nlohmann::json toJson() const
        {
            return { { "title", title }, { "revision", revision } };
        }

        static Note fromJson(const nlohmann::json& json)
        {
            return Note{ json.at("title").get<std::string>(), json.at("revision").get<int>() };
        }
    };

    std::string readText(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    class AppArchiveTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-archive");
            m_archivePath = m_directory / "backups" / "app.zip";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        // An application with a document, a setting and two files in the cloud.
        struct Installation
        {
            Installation(const fs::path& root, const int schemaVersion = 1) :
                file(root / "data" / "note.json", schemaVersion),
                settings(root / "config" / "settings.json")
            {}

            [[nodiscard]] AppArchiveContext context(const bool withAssets = true)
            {
                return { .appName = "notes", .appVersion = "1.2.3", .settings = &settings, .assets = withAssets ? &assets : nullptr };
            }

            DocumentFile<Note> file;
            SettingsStore settings;
            FakeAssetStorage assets;
        };

        [[nodiscard]] Installation source() const
        {
            Installation installation(m_directory / "source");
            installation.settings.set<std::string>("theme", "dark");
            installation.assets.put("a.txt", "first file");
            installation.assets.put("b.txt", "second file", "text/markdown");
            return installation;
        }

        [[nodiscard]] Installation target(const std::string& name = "target") const
        {
            return Installation(m_directory / name);
        }

        void exportFrom(Installation& installation, const Note& document = Note{ "original", 5 }) const
        {
            const auto result = exportAppArchive(installation.file, document, installation.context(), m_archivePath, m_clock);
            ASSERT_TRUE(result) << result.error().message();
        }

        // Writes an archive by hand, to test what a normal export cannot produce.
        void writeCustomArchive(const std::string& manifest, const bool withDocument = true) const
        {
            auto writer = ZipWriter::create(m_archivePath, stapik::test::FakeClock::DEFAULT_START);
            ASSERT_TRUE(writer);
            if (withDocument)
                ASSERT_TRUE((*writer)->addText("document.json", R"({"schemaVersion": 1, "document": {"title": "t", "revision": 1}})"));
            ASSERT_TRUE((*writer)->addText("manifest.json", manifest));
            ASSERT_TRUE((*writer)->finish());
        }

        static std::string manifestWith(const std::string& extra = "", const std::string& app = "notes", const int version = 1)
        {
            return R"({"format": "stapik-archive", "formatVersion": )" + std::to_string(version) + R"(, "app": ")" + app
                + R"(", "appVersion": "1.0", "createdAt": "2026-01-15T10:30:00Z", "assets": [)" + extra + "]}";
        }

        fs::path m_directory;
        fs::path m_archivePath;
        stapik::test::FakeClock m_clock;
    };
}

TEST_F(AppArchiveTest, ExportContainsTheDocumentTheSettingsAndEveryAsset)
{
    auto installation = source();
    exportFrom(installation);

    const auto zip = ZipReader::open(m_archivePath);
    ASSERT_TRUE(zip) << zip.error().message();

    EXPECT_NE(zip->find("manifest.json"), nullptr);
    EXPECT_NE(zip->find("document.json"), nullptr);
    EXPECT_NE(zip->find("settings.json"), nullptr);
    EXPECT_EQ(zip->readText("assets/a.txt").valueOr(""), "first file");
    EXPECT_EQ(zip->readText("assets/b.txt").valueOr(""), "second file");
}

TEST_F(AppArchiveTest, ExportReportsWhatItWrote)
{
    auto installation = source();

    const auto info = exportAppArchive(installation.file, Note{ "n", 1 }, installation.context(), m_archivePath, m_clock);

    ASSERT_TRUE(info);
    EXPECT_EQ(info->appName, "notes");
    EXPECT_EQ(info->appVersion, "1.2.3");
    EXPECT_EQ(info->assetCount, 2U);
    EXPECT_EQ(info->createdAt, m_clock.now());
}

TEST_F(AppArchiveTest, TheArchiveRestoresTheWholeApplicationOnAnotherMachine)
{
    auto original = source();
    exportFrom(original, Note{ "original", 5 });

    auto restored = target();
    restored.settings.set<std::string>("theme", "classic");
    restored.settings.set<std::string>("leftover", "x");

    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(result->document.title, "original");
    EXPECT_EQ(result->document.revision, 5);
    EXPECT_EQ(result->info.appVersion, "1.2.3");
    EXPECT_EQ(result->summary.assetsUploaded, 2U);
    EXPECT_TRUE(result->summary.settingsRestored);

    EXPECT_EQ(restored.assets.text("a.txt"), "first file");
    EXPECT_EQ(restored.assets.files.at("b.txt").mimeType, "text/markdown");
    EXPECT_EQ(restored.file.load().document->title, "original");

    EXPECT_EQ(restored.settings.get<std::string>("theme", ""), "dark");
    EXPECT_FALSE(restored.settings.contains("leftover"));
    SettingsStore reloaded(m_directory / "target" / "config" / "settings.json");
    reloaded.load();
    EXPECT_EQ(reloaded.get<std::string>("theme", ""), "dark");
}

TEST_F(AppArchiveTest, RestoreKeepsTheReplacedDocumentAsABackup)
{
    auto original = source();
    exportFrom(original);

    auto restored = target();
    ASSERT_TRUE(restored.file.save(Note{ "current", 9 }));

    ASSERT_TRUE(importAppArchive(restored.file, m_archivePath, restored.context()));

    const auto backups = restored.file.listBackups();
    ASSERT_EQ(backups.size(), 1U);
    EXPECT_EQ(restored.file.loadBackup(backups.front()).document->title, "current");
}

TEST_F(AppArchiveTest, RestoreUploadsOnlyAssetsThatDiffer)
{
    auto original = source();
    exportFrom(original);

    auto restored = target();
    restored.assets.put("a.txt", "first file");
    restored.assets.put("b.txt", "changed in the meantime");
    restored.assets.put("only-in-cloud.txt", "newer than the backup");

    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_TRUE(result);
    EXPECT_EQ(result->summary.assetsUnchanged, 1U);
    EXPECT_EQ(result->summary.assetsUploaded, 1U);
    EXPECT_EQ(restored.assets.uploaded, std::vector<std::string>{ "b.txt" });
    EXPECT_EQ(restored.assets.text("b.txt"), "second file");
    EXPECT_EQ(restored.assets.text("only-in-cloud.txt"), "newer than the backup");
}

TEST_F(AppArchiveTest, RestoreWithoutAssetStorageReportsTheAssetsItCouldNotRestore)
{
    auto original = source();
    exportFrom(original);

    auto restored = target();
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context(false));

    ASSERT_TRUE(result);
    EXPECT_EQ(result->summary.assetsNotRestored, 2U);
    EXPECT_EQ(result->summary.assetsUploaded, 0U);
    EXPECT_EQ(restored.file.load().document->title, "original");
}

TEST_F(AppArchiveTest, AnApplicationWithoutSettingsOrAssetsCanBeArchived)
{
    Installation installation(m_directory / "plain");
    AppArchiveContext context{ .appName = "notes", .appVersion = "1" };

    ASSERT_TRUE(exportAppArchive(installation.file, Note{ "plain", 1 }, context, m_archivePath, m_clock));

    const auto result = importAppArchive(installation.file, m_archivePath, context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->document.title, "plain");
    EXPECT_FALSE(result->summary.settingsRestored);
}

TEST_F(AppArchiveTest, AnArchiveOfAnotherApplicationIsRefusedAndNothingChanges)
{
    auto original = source();
    exportFrom(original);

    auto restored = target();
    ASSERT_TRUE(restored.file.save(Note{ "current", 9 }));
    auto context = restored.context();
    context.appName = "groceries";

    const auto result = importAppArchive(restored.file, m_archivePath, context);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::WrongApplication);
    EXPECT_EQ(restored.file.load().document->title, "current");
    EXPECT_TRUE(restored.assets.files.empty());
}

TEST_F(AppArchiveTest, ADamagedAssetRefusesTheWholeArchiveBeforeAnythingChanges)
{
    auto original = source();
    exportFrom(original);

    auto bytes = readText(m_archivePath);
    const auto zip = ZipReader::open(m_archivePath);
    const auto* asset = zip->find("assets/b.txt");
    ASSERT_NE(asset, nullptr);
    bytes[asset->localHeaderOffset + 30 + asset->name.size()] ^= 0x55;
    std::ofstream(m_archivePath, std::ios::binary | std::ios::trunc) << bytes;

    auto restored = target();
    ASSERT_TRUE(restored.file.save(Note{ "current", 9 }));

    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Corrupted);
    EXPECT_TRUE(restored.assets.uploaded.empty());
    EXPECT_EQ(restored.file.load().document->title, "current");
}

TEST_F(AppArchiveTest, AZipWithoutAManifestIsNotAnApplicationArchive)
{
    {
        auto writer = ZipWriter::create(m_archivePath, stapik::test::FakeClock::DEFAULT_START);
        ASSERT_TRUE(writer);
        ASSERT_TRUE((*writer)->addText("photo.txt", "holiday"));
        ASSERT_TRUE((*writer)->finish());
    }

    auto restored = target();
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::NotAnArchive);
}

TEST_F(AppArchiveTest, AManifestOfAnotherFormatIsNotAnApplicationArchive)
{
    writeCustomArchive(R"({"format": "something-else", "formatVersion": 1})");

    auto restored = target();
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::NotAnArchive);
}

TEST_F(AppArchiveTest, AnArchiveFromANewerFormatIsRefused)
{
    writeCustomArchive(manifestWith("", "notes", ARCHIVE_FORMAT_VERSION + 1));

    auto restored = target();
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Unsupported);
}

TEST_F(AppArchiveTest, AnArchiveWithoutADocumentIsCorrupted)
{
    writeCustomArchive(manifestWith(), false);

    auto restored = target();
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Corrupted);
}

TEST_F(AppArchiveTest, AFileNameThatLeavesTheAssetsFolderIsCorrupted)
{
    writeCustomArchive(manifestWith(R"({"filename": "../escape.txt", "mimeType": "text/plain", "sizeBytes": 1, "checksumSha256": "00"})"));

    auto restored = target();
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Corrupted);
    EXPECT_TRUE(restored.assets.files.empty());
}

TEST_F(AppArchiveTest, ADocumentFromANewerVersionOfTheApplicationIsRefusedAndNothingChanges)
{
    Installation future(m_directory / "future", 3);
    ASSERT_TRUE(exportAppArchive(future.file, Note{ "future", 1 }, future.context(), m_archivePath, m_clock));

    auto restored = target();
    ASSERT_TRUE(restored.file.save(Note{ "current", 9 }));
    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Unsupported);
    EXPECT_EQ(restored.file.load().document->title, "current");
    EXPECT_TRUE(restored.assets.files.empty());
}

TEST_F(AppArchiveTest, AFailedUploadLeavesTheDocumentAndSettingsAlone)
{
    auto original = source();
    exportFrom(original);

    auto restored = target();
    ASSERT_TRUE(restored.file.save(Note{ "current", 9 }));
    restored.settings.set<std::string>("theme", "classic");
    restored.assets.tooLarge.insert("b.txt");

    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Cloud);
    EXPECT_EQ(restored.file.load().document->title, "current");
    EXPECT_EQ(restored.settings.get<std::string>("theme", ""), "classic");
}

TEST_F(AppArchiveTest, AnUnreachableCloudFailsTheRestoreBeforeTheDocumentIsReplaced)
{
    auto original = source();
    exportFrom(original);

    auto restored = target();
    ASSERT_TRUE(restored.file.save(Note{ "current", 9 }));
    restored.assets.unreachable = true;

    const auto result = importAppArchive(restored.file, m_archivePath, restored.context());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Cloud);
    EXPECT_EQ(restored.file.load().document->title, "current");
}

TEST_F(AppArchiveTest, AnUnreachableCloudFailsTheExportAndLeavesNoFile)
{
    auto installation = source();
    installation.assets.unreachable = true;

    const auto result = exportAppArchive(installation.file, Note{ "n", 1 }, installation.context(), m_archivePath, m_clock);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Cloud);
    EXPECT_FALSE(fs::exists(m_archivePath));
}

TEST_F(AppArchiveTest, AnAssetThatDisappearsDuringTheExportFailsItInsteadOfSkippingIt)
{
    auto installation = source();
    installation.assets.vanishing.insert("b.txt");

    const auto result = exportAppArchive(installation.file, Note{ "n", 1 }, installation.context(), m_archivePath, m_clock);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Cloud);
    EXPECT_FALSE(fs::exists(m_archivePath));
}

TEST_F(AppArchiveTest, AFileNameThatCannotBeStoredFailsTheExport)
{
    auto installation = source();
    installation.assets.put("folder/file.txt", "x");

    const auto result = exportAppArchive(installation.file, Note{ "n", 1 }, installation.context(), m_archivePath, m_clock);

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().kind, ArchiveErrorKind::Unsupported);
    EXPECT_FALSE(fs::exists(m_archivePath));
}

TEST_F(AppArchiveTest, ExportDoesNotTouchTheDataFiles)
{
    auto installation = source();
    ASSERT_TRUE(installation.file.save(Note{ "stored", 2 }));
    const auto before = readText(m_directory / "source" / "data" / "note.json");

    exportFrom(installation, Note{ "unsaved edit", 3 });

    EXPECT_EQ(readText(m_directory / "source" / "data" / "note.json"), before);
}

TEST_F(AppArchiveTest, ExportReplacesInvalidUtf8InTheManifestInsteadOfFailing)
{
    auto installation = source();
    installation.assets.put("c.txt", "third file", "text/bad\xff");

    exportFrom(installation);

    const auto zip = ZipReader::open(m_archivePath);
    ASSERT_TRUE(zip) << zip.error().message();
    EXPECT_NE(zip->readText("manifest.json").valueOr("").find("text/bad\xEF\xBF\xBD"), std::string::npos);
}
