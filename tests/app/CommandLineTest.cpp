#include "stapik/app/CommandLine.hpp"

#include "stapik/storage/AppPaths.hpp"
#include "support/TemporaryDirectory.hpp"

#include <glibmm/init.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

namespace
{
    namespace fs = std::filesystem;
    using namespace stapik::app;

    Glib::RefPtr<Glib::VariantDict> makeValues()
    {
        Glib::init();
        return Glib::VariantDict::create();
    }

    void setFlag(Glib::VariantDict& values, const char* key)
    {
        values.insert_value(key, Glib::Variant<bool>::create(true));
    }

    void setString(Glib::VariantDict& values, const char* key, const char* text)
    {
        values.insert_value(key, Glib::Variant<Glib::ustring>::create(text));
    }

    class ApplyCommandLineTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-cli");
            m_originalLevel = stapik::log::level();
        }

        void TearDown() override
        {
            AppPaths::clearRootOverride();
            stapik::log::setLevel(m_originalLevel);

            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;

    private:
        stapik::log::Level m_originalLevel{};
    };
}

TEST(CommandLineTest, NoFlagsGiveDefaultOptions)
{
    const auto result = readCommandLineOptions(*makeValues());

    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result.options.showVersion);
    EXPECT_FALSE(result.options.logLevel.has_value());
    EXPECT_FALSE(result.options.dataDir.has_value());
    EXPECT_FALSE(result.options.portable);
    EXPECT_FALSE(result.options.resetSettings);
    EXPECT_FALSE(result.options.safeMode);
    EXPECT_TRUE(result.options.files.empty());
}

TEST(CommandLineTest, ReadsBooleanFlags)
{
    const auto values = makeValues();
    setFlag(*values, "version");
    setFlag(*values, "portable");
    setFlag(*values, "reset-settings");
    setFlag(*values, "safe-mode");

    const auto result = readCommandLineOptions(*values);

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.options.showVersion);
    EXPECT_TRUE(result.options.portable);
    EXPECT_TRUE(result.options.resetSettings);
    EXPECT_TRUE(result.options.safeMode);
}

TEST(CommandLineTest, FilesAreMadeAbsoluteAndKeepTheirOrder)
{
    const auto values = makeValues();
    values->insert_value("", Glib::Variant<std::vector<std::string>>::create({"first.txt", "/tmp/second.txt", ""}));

    const auto result = readCommandLineOptions(*values);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.options.files.size(), 2U);
    EXPECT_TRUE(result.options.files[0].is_absolute());
    EXPECT_EQ(result.options.files[0].filename(), "first.txt");
    EXPECT_EQ(result.options.files[1], fs::path("/tmp/second.txt"));
}

TEST(CommandLineTest, ReadsLogLevel)
{
    const auto values = makeValues();
    setString(*values, "log-level", "Debug");

    const auto result = readCommandLineOptions(*values);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.options.logLevel, stapik::log::Level::Debug);
}

TEST(CommandLineTest, RejectsUnknownLogLevel)
{
    const auto values = makeValues();
    setString(*values, "log-level", "verbose");

    const auto result = readCommandLineOptions(*values);

    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.error.find("verbose"), std::string::npos);
}

TEST(CommandLineTest, DataDirIsMadeAbsolute)
{
    const auto values = makeValues();
    setString(*values, "data-dir", "relative/data");

    const auto result = readCommandLineOptions(*values);

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.options.dataDir.has_value());
    EXPECT_TRUE(result.options.dataDir->is_absolute());
    EXPECT_EQ(result.options.dataDir->filename(), "data");
}

TEST(CommandLineTest, RejectsEmptyDataDir)
{
    const auto values = makeValues();
    setString(*values, "data-dir", "");

    EXPECT_FALSE(readCommandLineOptions(*values).ok());
}

TEST(CommandLineTest, RejectsDataDirTogetherWithPortable)
{
    const auto values = makeValues();
    setString(*values, "data-dir", "/tmp/stapik");
    setFlag(*values, "portable");

    EXPECT_FALSE(readCommandLineOptions(*values).ok());
}

TEST(CommandLineTest, VersionLineUsesDisplayNameWhenAvailable)
{
    AppInfo info;
    info.internalName = "stapikexample";
    info.version = "1.2.3";
    EXPECT_EQ(versionLine(info), "stapikexample 1.2.3");

    info.displayName = "Stapik Example";
    EXPECT_EQ(versionLine(info), "Stapik Example 1.2.3");
}

TEST_F(ApplyCommandLineTest, AppliesLogLevel)
{
    CommandLineOptions options;
    options.logLevel = stapik::log::Level::Error;

    applyCommandLineOptions(options, "stapik-cli-level-test");

    EXPECT_EQ(stapik::log::level(), stapik::log::Level::Error);
}

TEST_F(ApplyCommandLineTest, DataDirRelocatesUserDirectories)
{
    CommandLineOptions options;
    options.dataDir = m_directory;

    applyCommandLineOptions(options, "stapik-cli-datadir-test");

    EXPECT_EQ(AppPaths::userConfigDir("stapik-cli-datadir-test"), m_directory / "config");
}

TEST_F(ApplyCommandLineTest, PortableUsesFolderNextToTheExecutable)
{
    CommandLineOptions options;
    options.portable = true;

    applyCommandLineOptions(options, "stapik-cli-portable-test");

    EXPECT_EQ(AppPaths::userDataDir("stapik-cli-portable-test"), AppPaths::portableRoot() / "data");
}

TEST_F(ApplyCommandLineTest, ResetSettingsSetsAsideSettingsAndLegacyFiles)
{
    const auto settingsFile = m_directory / "config" / "settings.json";
    const auto legacyFile = m_directory / "data" / "theme.txt";
    for (const auto& file : { settingsFile, legacyFile })
    {
        fs::create_directories(file.parent_path());
        std::ofstream(file) << "{\"version\":1,\"settings\":{}}";
    }

    CommandLineOptions options;
    options.dataDir = m_directory;
    options.resetSettings = true;

    applyCommandLineOptions(options, "stapik-cli-reset-test");

    EXPECT_FALSE(fs::exists(settingsFile));
    EXPECT_TRUE(fs::exists(fs::path(settingsFile) += ".bak"));
    EXPECT_FALSE(fs::exists(legacyFile));
    EXPECT_TRUE(fs::exists(fs::path(legacyFile) += ".bak"));
}
