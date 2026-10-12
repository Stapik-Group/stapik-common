#include "stapik/settings/SettingsStore.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::settings::LoadStatus;
    using stapik::settings::SettingsStore;

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

    class SettingsStoreTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-settings-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
            m_path = m_directory / "config" / "settings.json";
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
        fs::path m_path;
    };
}

TEST_F(SettingsStoreTest, LoadOfMissingFileReportsMissingAndKeepsDefaults)
{
    SettingsStore store(m_path);

    EXPECT_EQ(store.load(), LoadStatus::Missing);
    EXPECT_EQ(store.get<std::string>("theme", "classic"), "classic");
    EXPECT_FALSE(store.contains("theme"));
}

TEST_F(SettingsStoreTest, SavedValuesSurviveReload)
{
    SettingsStore writer(m_path);
    writer.set<std::string>("locale", "pl");
    writer.set<int>("fontSize", 14);
    writer.set<bool>("showHints", false);
    ASSERT_TRUE(writer.save());

    SettingsStore reader(m_path);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);

    EXPECT_EQ(reader.get<std::string>("locale", "en"), "pl");
    EXPECT_EQ(reader.get<int>("fontSize", 0), 14);
    EXPECT_EQ(reader.get<bool>("showHints", true), false);
}

TEST_F(SettingsStoreTest, SaveCreatesMissingDirectories)
{
    SettingsStore store(m_path);
    store.set<int>("answer", 42);

    ASSERT_TRUE(store.save());

    EXPECT_TRUE(fs::exists(m_path));
}

TEST_F(SettingsStoreTest, GetReturnsDefaultForMissingKey)
{
    const SettingsStore store(m_path);

    EXPECT_EQ(store.get<int>("missing", 7), 7);
}

TEST_F(SettingsStoreTest, GetReturnsDefaultForValueOfAnotherType)
{
    SettingsStore store(m_path);
    store.set<std::string>("fontSize", "large");

    EXPECT_EQ(store.get<int>("fontSize", 12), 12);
}

TEST_F(SettingsStoreTest, RemoveDeletesKey)
{
    SettingsStore store(m_path);
    store.set<int>("answer", 42);

    store.remove("answer");

    EXPECT_FALSE(store.contains("answer"));
    EXPECT_EQ(store.get<int>("answer", 0), 0);
}

TEST_F(SettingsStoreTest, LoadReplacesUnsavedInMemoryValues)
{
    SettingsStore writer(m_path);
    writer.set<int>("a", 1);
    ASSERT_TRUE(writer.save());

    SettingsStore reader(m_path);
    reader.set<int>("b", 2);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);

    EXPECT_TRUE(reader.contains("a"));
    EXPECT_FALSE(reader.contains("b"));
}

TEST_F(SettingsStoreTest, CorruptedJsonIsMovedAsideAndStoreStartsEmpty)
{
    writeText(m_path, "{ this is not json");

    SettingsStore store(m_path);

    EXPECT_EQ(store.load(), LoadStatus::Corrupted);
    EXPECT_FALSE(fs::exists(m_path));
    EXPECT_EQ(readText(fs::path(m_path.string() + ".corrupt")), "{ this is not json");
    EXPECT_FALSE(store.contains("anything"));
}

TEST_F(SettingsStoreTest, UnexpectedStructureIsTreatedAsCorrupted)
{
    writeText(m_path, R"({"version": 1, "settings": [1, 2, 3]})");

    SettingsStore store(m_path);

    EXPECT_EQ(store.load(), LoadStatus::Corrupted);
    EXPECT_TRUE(fs::exists(fs::path(m_path.string() + ".corrupt")));
}

TEST_F(SettingsStoreTest, SavingAfterCorruptionDoesNotDestroyTheOriginal)
{
    writeText(m_path, "garbage");

    SettingsStore store(m_path);
    ASSERT_EQ(store.load(), LoadStatus::Corrupted);
    store.set<int>("fresh", 1);
    ASSERT_TRUE(store.save());

    EXPECT_EQ(readText(fs::path(m_path.string() + ".corrupt")), "garbage");
    EXPECT_TRUE(fs::exists(m_path));
}

TEST_F(SettingsStoreTest, NewerSchemaVersionIsReadAndPreserved)
{
    writeText(m_path, R"({"version": 5, "settings": {"locale": "de"}})");

    SettingsStore store(m_path, 1);

    ASSERT_EQ(store.load(), LoadStatus::Loaded);
    EXPECT_EQ(store.schemaVersion(), 5);
    EXPECT_EQ(store.get<std::string>("locale", "en"), "de");

    ASSERT_TRUE(store.save());
    SettingsStore reader(m_path, 1);
    ASSERT_EQ(reader.load(), LoadStatus::Loaded);
    EXPECT_EQ(reader.schemaVersion(), 5);
}

TEST_F(SettingsStoreTest, OlderSchemaVersionKeepsCurrentVersion)
{
    writeText(m_path, R"({"version": 1, "settings": {"locale": "de"}})");

    SettingsStore store(m_path, 3);

    ASSERT_EQ(store.load(), LoadStatus::Loaded);
    EXPECT_EQ(store.schemaVersion(), 3);
}

TEST_F(SettingsStoreTest, ResetClearsValuesAndKeepsBackupOfTheFile)
{
    writeText(m_path, R"({"version":1,"settings":{"theme":"dark"}})");
    SettingsStore store(m_path);
    store.load();

    EXPECT_TRUE(store.reset());

    EXPECT_FALSE(store.contains("theme"));
    EXPECT_FALSE(fs::exists(m_path));
    EXPECT_TRUE(fs::exists(fs::path(m_path) += ".bak"));
}

TEST_F(SettingsStoreTest, ResetWithoutFileSucceeds)
{
    SettingsStore store(m_path);

    EXPECT_TRUE(store.reset());
}

TEST_F(SettingsStoreTest, SaveReportsWhyTheFileCannotBeWritten)
{
    writeText(m_directory / "blocker", "i am a file");
    const SettingsStore store(m_directory / "blocker" / "settings.json");

    const auto result = store.save();

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().step, "create directory");
    EXPECT_EQ(result.error().path, m_directory / "blocker");
}

TEST_F(SettingsStoreTest, ResetReportsWhyTheFileCannotBeMovedAside)
{
    writeText(m_path, R"({"version":1,"settings":{"theme":"dark"}})");
    auto occupiedBackup = m_path;
    occupiedBackup += ".bak";
    writeText(occupiedBackup / "keep", "occupied");
    SettingsStore store(m_path);
    store.load();

    const auto result = store.reset();

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().step, "move aside");
    EXPECT_EQ(result.error().path, m_path);
    EXPECT_TRUE(fs::exists(m_path));
}

TEST_F(SettingsStoreTest, SerializedSettingsAreTheContentOfTheSavedFile)
{
    SettingsStore store(m_path);
    store.set<std::string>("theme", "dark");
    ASSERT_TRUE(store.save());

    EXPECT_EQ(store.serialize(), readText(m_path));
}

TEST_F(SettingsStoreTest, ReplaceFromTextSwapsAllValuesWithoutWriting)
{
    SettingsStore store(m_path);
    store.set<std::string>("theme", "dark");
    store.set<int>("old", 1);

    SettingsStore source(m_directory / "other" / "settings.json");
    source.set<std::string>("theme", "modern");
    source.set<std::string>("locale", "pl");

    ASSERT_TRUE(store.replaceFromText(source.serialize()));

    EXPECT_EQ(store.get<std::string>("theme", ""), "modern");
    EXPECT_EQ(store.get<std::string>("locale", ""), "pl");
    EXPECT_FALSE(store.contains("old"));
    EXPECT_FALSE(fs::exists(m_path));
}

TEST_F(SettingsStoreTest, ReplaceFromTextRefusesInvalidTextAndKeepsTheValues)
{
    SettingsStore store(m_path);
    store.set<std::string>("theme", "dark");

    EXPECT_FALSE(store.replaceFromText("not json"));
    EXPECT_FALSE(store.replaceFromText(R"({"version": 1})"));
    EXPECT_FALSE(store.replaceFromText(R"({"version": 1, "settings": []})"));

    EXPECT_EQ(store.get<std::string>("theme", ""), "dark");
}

TEST_F(SettingsStoreTest, ReplaceFromTextKeepsTheNewerSchemaVersionOfTheText)
{
    SettingsStore store(m_path, 1);

    ASSERT_TRUE(store.replaceFromText(R"({"version": 4, "settings": {"theme": "dark"}})"));

    EXPECT_EQ(store.schemaVersion(), 4);
}
