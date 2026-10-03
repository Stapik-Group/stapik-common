#include "stapik/theme/ThemeRegistry.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::theme::ThemeRegistry;

    void writeText(const fs::path& path, const std::string& text = "window {}")
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        file << text;
    }

    class ThemeRegistryTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            auto pattern = (fs::temp_directory_path() / "stapik-themes-XXXXXX").string();
            ASSERT_NE(mkdtemp(pattern.data()), nullptr);
            m_directory = pattern;
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        fs::path m_directory;
    };
}

TEST_F(ThemeRegistryTest, DiscoversThemesInTheThemesSubdirectory)
{
    writeText(m_directory / "themes" / "modern.css");
    writeText(m_directory / "themes" / "classic.css");

    ThemeRegistry registry;
    registry.addDirectory(m_directory);

    ASSERT_EQ(registry.themes().size(), 2u);
    EXPECT_EQ(registry.themes()[0].id, "classic");
    EXPECT_EQ(registry.themes()[1].id, "modern");
    EXPECT_EQ(registry.themes()[1].nameKey, "theme.modern");
    ASSERT_EQ(registry.themes()[1].files.size(), 1u);
    EXPECT_EQ(registry.themes()[1].files.front(), m_directory / "themes" / "modern.css");
}

TEST_F(ThemeRegistryTest, DiscoversLegacyStylePrefixFiles)
{
    writeText(m_directory / "style-classic.css");
    writeText(m_directory / "style-classic-pink.css");

    ThemeRegistry registry;
    registry.addDirectory(m_directory);

    ASSERT_EQ(registry.themes().size(), 2u);
    EXPECT_TRUE(registry.contains("classic"));
    EXPECT_TRUE(registry.contains("classic-pink"));
    EXPECT_EQ(registry.find("classic-pink")->files.front(), m_directory / "style-classic-pink.css");
}

TEST_F(ThemeRegistryTest, BothLayoutsInOneDirectoryAreLayered)
{
    writeText(m_directory / "themes" / "modern.css");
    writeText(m_directory / "style-modern.css");

    ThemeRegistry registry;
    registry.addDirectory(m_directory);

    const auto* modern = registry.find("modern");
    ASSERT_NE(modern, nullptr);
    ASSERT_EQ(modern->files.size(), 2u);
    EXPECT_EQ(modern->files[0], m_directory / "themes" / "modern.css");
    EXPECT_EQ(modern->files[1], m_directory / "style-modern.css");
}

TEST_F(ThemeRegistryTest, LaterDirectoryIsLayeredOnTopOfTheEarlierOne)
{
    const auto base = m_directory / "base";
    const auto app = m_directory / "app";
    writeText(base / "themes" / "classic.css");
    writeText(base / "themes" / "modern.css");
    writeText(app / "style-classic.css");
    writeText(app / "style-custom.css");

    ThemeRegistry registry;
    registry.addDirectory(base);
    registry.addDirectory(app);

    ASSERT_EQ(registry.themes().size(), 3u);
    const auto* classic = registry.find("classic");
    ASSERT_NE(classic, nullptr);
    ASSERT_EQ(classic->files.size(), 2u);
    EXPECT_EQ(classic->files[0], base / "themes" / "classic.css");
    EXPECT_EQ(classic->files[1], app / "style-classic.css");
    EXPECT_EQ(registry.find("custom")->files.size(), 1u);
    EXPECT_EQ(registry.find("modern")->files.size(), 1u);
}

TEST_F(ThemeRegistryTest, IdIsLowercased)
{
    writeText(m_directory / "themes" / "Modern.css");

    ThemeRegistry registry;
    registry.addDirectory(m_directory);

    EXPECT_TRUE(registry.contains("modern"));
    EXPECT_FALSE(registry.contains("Modern"));
}

TEST_F(ThemeRegistryTest, IgnoresOtherFilesAndPlainStylePrefix)
{
    writeText(m_directory / "themes" / "classic.css");
    writeText(m_directory / "themes" / "notes.txt");
    writeText(m_directory / "themes" / ".hidden.css");
    writeText(m_directory / "style-.css");
    writeText(m_directory / "other.css");
    writeText(m_directory / "style-modern.txt");

    ThemeRegistry registry;
    registry.addDirectory(m_directory);

    ASSERT_EQ(registry.themes().size(), 1u);
    EXPECT_EQ(registry.themes().front().id, "classic");
}

TEST_F(ThemeRegistryTest, MissingDirectoryIsIgnored)
{
    ThemeRegistry registry;
    registry.addDirectory(m_directory / "does-not-exist");

    EXPECT_TRUE(registry.themes().empty());
    EXPECT_EQ(registry.find("classic"), nullptr);
}

TEST_F(ThemeRegistryTest, DirectoryWithoutThemesIsFine)
{
    ThemeRegistry registry;
    registry.addDirectory(m_directory);

    EXPECT_TRUE(registry.themes().empty());
}
