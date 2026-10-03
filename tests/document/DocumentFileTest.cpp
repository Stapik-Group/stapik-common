#include "stapik/document/DocumentFile.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using stapik::document::DocumentFile;
    using stapik::document::LoadStatus;
    using stapik::document::SchemaMigrator;

    struct Note
    {
        std::string title;
        int revision = 0;

        [[nodiscard]] nlohmann::json toJson() const
        {
            nlohmann::json json = nlohmann::json::object();
            json["title"] = title;
            json["revision"] = revision;
            return json;
        }

        static Note fromJson(const nlohmann::json& json)
        {
            return Note{ json.at("title").get<std::string>(), json.at("revision").get<int>() };
        }
    };

    static_assert(stapik::document::PersistableDocument<Note>);

    void writeText(const fs::path& path, const std::string& text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
    }

    std::string readText(const fs::path& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    class DocumentFileTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-document-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
            m_path = m_directory / "data" / "note.json";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        [[nodiscard]] std::vector<fs::path> quarantinedFiles() const
        {
            std::vector<fs::path> found;
            if (!fs::exists(m_path.parent_path()))
                return found;

            for (const auto& entry : fs::directory_iterator(m_path.parent_path()))
            {
                if (entry.path().filename().string().starts_with("note.json.corrupt-"))
                    found.push_back(entry.path());
            }
            return found;
        }

        [[nodiscard]] fs::path backup(const std::string& suffix) const
        {
            return fs::path(m_path.string() + suffix);
        }

        fs::path m_directory;
        fs::path m_path;
    };
}

TEST_F(DocumentFileTest, LoadOfMissingFileReportsMissing)
{
    DocumentFile<Note> file(m_path, 1);

    const auto result = file.load();

    EXPECT_EQ(result.status, LoadStatus::Missing);
    EXPECT_FALSE(result.document.has_value());
}

TEST_F(DocumentFileTest, SavedDocumentIsLoadedBack)
{
    DocumentFile<Note> file(m_path, 1);
    ASSERT_TRUE(file.save(Note{ "hello", 7 }));

    const auto result = file.load();

    ASSERT_EQ(result.status, LoadStatus::Loaded);
    ASSERT_TRUE(result.document.has_value());
    EXPECT_EQ(result.document->title, "hello");
    EXPECT_EQ(result.document->revision, 7);
    EXPECT_FALSE(result.migrated);
}

TEST_F(DocumentFileTest, SavedFileContainsSchemaVersionEnvelope)
{
    DocumentFile<Note> file(m_path, 4);
    ASSERT_TRUE(file.save(Note{ "hello", 1 }));

    const auto text = readText(m_path);

    EXPECT_NE(text.find("schemaVersion"), std::string::npos);
    EXPECT_NE(text.find("document"), std::string::npos);
}

TEST_F(DocumentFileTest, SaveLeavesNoTemporaryFiles)
{
    DocumentFile<Note> file(m_path, 1);
    file.setBackupCount(0);

    ASSERT_TRUE(file.save(Note{ "a", 1 }));
    ASSERT_TRUE(file.save(Note{ "b", 2 }));

    std::size_t entries = 0;
    for (const auto& entry : fs::directory_iterator(m_path.parent_path()))
    {
        static_cast<void>(entry);
        ++entries;
    }
    EXPECT_EQ(entries, 1u);
}

TEST_F(DocumentFileTest, InvalidJsonIsQuarantinedNotReplaced)
{
    writeText(m_path, "{ not json");
    DocumentFile<Note> file(m_path, 1);

    const auto result = file.load();

    EXPECT_EQ(result.status, LoadStatus::Corrupted);
    EXPECT_FALSE(result.document.has_value());
    EXPECT_FALSE(fs::exists(m_path));
    const auto quarantined = quarantinedFiles();
    ASSERT_EQ(quarantined.size(), 1u);
    EXPECT_EQ(readText(quarantined.front()), "{ not json");
}

TEST_F(DocumentFileTest, ValidJsonWithWrongContentIsQuarantined)
{
    writeText(m_path, R"({"schemaVersion": 1, "document": {"title": "no revision"}})");
    DocumentFile<Note> file(m_path, 1);

    const auto result = file.load();

    EXPECT_EQ(result.status, LoadStatus::Corrupted);
    EXPECT_FALSE(fs::exists(m_path));
    EXPECT_EQ(quarantinedFiles().size(), 1u);
}

TEST_F(DocumentFileTest, SavingAfterCorruptionKeepsTheQuarantinedCopy)
{
    writeText(m_path, "garbage");
    DocumentFile<Note> file(m_path, 1);
    ASSERT_EQ(file.load().status, LoadStatus::Corrupted);

    ASSERT_TRUE(file.save(Note{ "fresh", 1 }));

    ASSERT_EQ(quarantinedFiles().size(), 1u);
    EXPECT_EQ(readText(quarantinedFiles().front()), "garbage");
    EXPECT_EQ(file.load().status, LoadStatus::Loaded);
}

TEST_F(DocumentFileTest, LegacyFileWithoutEnvelopeIsLoadedAsIsWhenNoStepZero)
{
    writeText(m_path, R"({"title": "legacy", "revision": 3})");
    DocumentFile<Note> file(m_path, 1);

    const auto result = file.load();

    ASSERT_EQ(result.status, LoadStatus::Loaded);
    EXPECT_EQ(result.document->title, "legacy");
    EXPECT_FALSE(result.migrated);
}

TEST_F(DocumentFileTest, LegacyFileIsMigratedFromVersionZeroWhenStepIsRegistered)
{
    writeText(m_path, R"({"name": "legacy", "revision": 3})");
    SchemaMigrator migrator(1);
    migrator.addStep(0, [](nlohmann::json& document)
    {
        document["title"] = document.at("name");
        document.erase("name");
    });
    DocumentFile<Note> file(m_path, migrator);

    const auto result = file.load();

    ASSERT_EQ(result.status, LoadStatus::Loaded);
    EXPECT_TRUE(result.migrated);
    EXPECT_EQ(result.fileVersion, 0);
    EXPECT_EQ(result.document->title, "legacy");
}

TEST_F(DocumentFileTest, OlderSchemaVersionIsMigrated)
{
    writeText(m_path, R"({"schemaVersion": 1, "document": {"name": "old", "revision": 2}})");
    SchemaMigrator migrator(2);
    migrator.addStep(1, [](nlohmann::json& document)
    {
        document["title"] = document.at("name");
        document.erase("name");
    });
    DocumentFile<Note> file(m_path, migrator);

    const auto result = file.load();

    ASSERT_EQ(result.status, LoadStatus::Loaded);
    EXPECT_TRUE(result.migrated);
    EXPECT_EQ(result.fileVersion, 1);
    EXPECT_EQ(result.document->title, "old");
}

TEST_F(DocumentFileTest, MigrationDoesNotRewriteTheFileOnLoad)
{
    const std::string original = R"({"schemaVersion": 1, "document": {"name": "old", "revision": 2}})";
    writeText(m_path, original);
    SchemaMigrator migrator(2);
    migrator.addStep(1, [](nlohmann::json& document)
    {
        document["title"] = document.at("name");
        document.erase("name");
    });
    DocumentFile<Note> file(m_path, migrator);

    ASSERT_EQ(file.load().status, LoadStatus::Loaded);

    EXPECT_EQ(readText(m_path), original);
}

TEST_F(DocumentFileTest, NewerSchemaVersionIsReportedAndLeftUntouched)
{
    const std::string original = R"({"schemaVersion": 9, "document": {"title": "future", "revision": 1}})";
    writeText(m_path, original);
    DocumentFile<Note> file(m_path, 1);

    const auto result = file.load();

    EXPECT_EQ(result.status, LoadStatus::NewerVersion);
    EXPECT_FALSE(result.document.has_value());
    EXPECT_EQ(result.fileVersion, 9);
    EXPECT_EQ(readText(m_path), original);
    EXPECT_TRUE(quarantinedFiles().empty());
}

TEST_F(DocumentFileTest, MissingMigrationStepIsReportedAndFileLeftUntouched)
{
    const std::string original = R"({"schemaVersion": 1, "document": {"title": "x", "revision": 1}})";
    writeText(m_path, original);
    DocumentFile<Note> file(m_path, 3);

    const auto result = file.load();

    EXPECT_EQ(result.status, LoadStatus::MigrationFailed);
    EXPECT_EQ(readText(m_path), original);
    EXPECT_TRUE(quarantinedFiles().empty());
}

TEST_F(DocumentFileTest, SaveKeepsPreviousVersionAsBackup)
{
    DocumentFile<Note> file(m_path, 1);

    ASSERT_TRUE(file.save(Note{ "first", 1 }));
    ASSERT_TRUE(file.save(Note{ "second", 2 }));

    ASSERT_TRUE(fs::exists(backup(".bak")));
    EXPECT_NE(readText(backup(".bak")).find("first"), std::string::npos);
    EXPECT_NE(readText(m_path).find("second"), std::string::npos);
}

TEST_F(DocumentFileTest, FirstSaveCreatesNoBackup)
{
    DocumentFile<Note> file(m_path, 1);

    ASSERT_TRUE(file.save(Note{ "first", 1 }));

    EXPECT_FALSE(fs::exists(backup(".bak")));
}

TEST_F(DocumentFileTest, BackupsAreRotated)
{
    DocumentFile<Note> file(m_path, 1);
    file.setBackupCount(2);

    ASSERT_TRUE(file.save(Note{ "first", 1 }));
    ASSERT_TRUE(file.save(Note{ "second", 2 }));
    ASSERT_TRUE(file.save(Note{ "third", 3 }));

    EXPECT_NE(readText(backup(".bak")).find("second"), std::string::npos);
    EXPECT_NE(readText(backup(".bak.2")).find("first"), std::string::npos);
    EXPECT_NE(readText(m_path).find("third"), std::string::npos);
}

TEST_F(DocumentFileTest, ZeroBackupCountDisablesBackups)
{
    DocumentFile<Note> file(m_path, 1);
    file.setBackupCount(0);

    ASSERT_TRUE(file.save(Note{ "first", 1 }));
    ASSERT_TRUE(file.save(Note{ "second", 2 }));

    EXPECT_FALSE(fs::exists(backup(".bak")));
}

TEST_F(DocumentFileTest, BrokenFileDoesNotOverwriteGoodBackup)
{
    writeText(backup(".bak"), "good backup");
    writeText(m_path, "broken");
    DocumentFile<Note> file(m_path, 1);

    ASSERT_TRUE(file.save(Note{ "fresh", 1 }));

    EXPECT_EQ(readText(backup(".bak")), "good backup");
}

TEST_F(DocumentFileTest, ExportWritesAFileThatCanBeImported)
{
    DocumentFile<Note> file(m_path, 1);
    const auto exported = m_directory / "export" / "note-export.json";

    ASSERT_TRUE(file.exportTo(Note{ "shared", 5 }, exported));
    const auto imported = file.importFrom(exported);

    ASSERT_EQ(imported.status, LoadStatus::Loaded);
    EXPECT_EQ(imported.document->title, "shared");
    EXPECT_EQ(imported.document->revision, 5);
}

TEST_F(DocumentFileTest, ImportDoesNotTouchTheManagedFile)
{
    DocumentFile<Note> file(m_path, 1);
    ASSERT_TRUE(file.save(Note{ "mine", 1 }));
    const auto before = readText(m_path);

    const auto exported = m_directory / "other.json";
    ASSERT_TRUE(file.exportTo(Note{ "theirs", 2 }, exported));
    const auto imported = file.importFrom(exported);

    ASSERT_EQ(imported.status, LoadStatus::Loaded);
    EXPECT_EQ(readText(m_path), before);
}

TEST_F(DocumentFileTest, InvalidImportIsRejectedAndNothingIsMoved)
{
    DocumentFile<Note> file(m_path, 1);
    ASSERT_TRUE(file.save(Note{ "mine", 1 }));
    const auto before = readText(m_path);

    const auto bad = m_directory / "bad.json";
    writeText(bad, "definitely not json");
    const auto imported = file.importFrom(bad);

    EXPECT_EQ(imported.status, LoadStatus::Corrupted);
    EXPECT_FALSE(imported.document.has_value());
    EXPECT_TRUE(fs::exists(bad));
    EXPECT_EQ(readText(m_path), before);
    EXPECT_TRUE(quarantinedFiles().empty());
}
