#include "stapik/app/BundledRuntime.hpp"

#include "support/ScopedEnvironment.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using stapik::app::configureBundledRuntime;
    using stapik::app::rebasePixbufLoadersCache;

    // What gdk-pixbuf-query-loaders writes: a header, then one block per loader separated by blank lines.
    const std::string SAMPLE_CACHE =
        "# GdkPixbuf Image Loader Modules file\n"
        "# Automatically generated file, do not edit\n"
        "#\n"
        "\"C:\\\\msys64\\\\ucrt64\\\\lib\\\\gdk-pixbuf-2.0\\\\2.10.0\\\\loaders\\\\libpixbufloader-png.dll\"\n"
        "\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\n"
        "\"image/png\" \"\"\n"
        "\"png\" \"\"\n"
        "\"\\211PNG\\r\\n\\032\\n\" \"\" 100\n"
        "\n"
        "\"lib\\\\gdk-pixbuf-2.0\\\\2.10.0\\\\loaders\\\\libpixbufloader-svg.dll\"\n"
        "\"svg\" 6 \"gdk-pixbuf\" \"Scalable Vector Graphics\" \"LGPL\"\n"
        "\"image/svg+xml\" \"\"\n"
        "\"svg\" \"\"\n"
        "\" <svg\" \"*    \" 100\n"
        "\n";

    std::string readAll(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<std::string> linesOf(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        for (std::string line; std::getline(stream, line);)
            lines.push_back(line);
        return lines;
    }

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory()
        {
            auto pattern = (fs::temp_directory_path() / "stapik-bundled-XXXXXX").string();
            EXPECT_NE(mkdtemp(pattern.data()), nullptr);
            m_path = pattern;
        }

        ~TemporaryDirectory()
        {
            std::error_code errorCode;
            fs::remove_all(m_path, errorCode);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]] const fs::path& path() const { return m_path; }

    private:
        fs::path m_path;
    };

    class BundledRuntimeTest : public testing::Test
    {
    protected:
        stapik::test::ScopedEnvironment m_schemaDir{ "GSETTINGS_SCHEMA_DIR" };
        stapik::test::ScopedEnvironment m_dataDirs{ "XDG_DATA_DIRS" };
        stapik::test::ScopedEnvironment m_fontconfig{ "FONTCONFIG_PATH" };
        stapik::test::ScopedEnvironment m_pixbufFile{ "GDK_PIXBUF_MODULE_FILE" };

        void SetUp() override
        {
            unsetenv("GSETTINGS_SCHEMA_DIR");
            unsetenv("XDG_DATA_DIRS");
            unsetenv("FONTCONFIG_PATH");
            unsetenv("GDK_PIXBUF_MODULE_FILE");
        }

        static void createFile(const fs::path& file, const std::string& content = "")
        {
            fs::create_directories(file.parent_path());
            std::ofstream(file, std::ios::binary) << content;
        }
    };
}

TEST(RebasePixbufLoadersCacheTest, PointsEveryLoaderIntoTheGivenDirectory)
{
    const auto rebased = rebasePixbufLoadersCache(SAMPLE_CACHE, fs::path("/app/lib/loaders"));
    const auto lines = linesOf(rebased);

    EXPECT_EQ(lines[3], "\"/app/lib/loaders/libpixbufloader-png.dll\"");
    EXPECT_EQ(lines[9], "\"/app/lib/loaders/libpixbufloader-svg.dll\"");
}

TEST(RebasePixbufLoadersCacheTest, KeepsEveryOtherLineUntouched)
{
    const auto original = linesOf(SAMPLE_CACHE);
    const auto rebased = linesOf(rebasePixbufLoadersCache(SAMPLE_CACHE, fs::path("/app/lib/loaders")));

    ASSERT_EQ(rebased.size(), original.size());
    for (std::size_t index = 0; index < original.size(); ++index)
    {
        if (index == 3 || index == 9)
            continue;

        EXPECT_EQ(rebased[index], original[index]) << "line " << index;
    }
}

TEST(RebasePixbufLoadersCacheTest, EscapesBackslashesOfAWindowsDirectory)
{
    const auto rebased = rebasePixbufLoadersCache(SAMPLE_CACHE, fs::path("C:\\Users\\Zosia\\app\\loaders"));

    EXPECT_NE(rebased.find("\"C:\\\\Users\\\\Zosia\\\\app\\\\loaders/libpixbufloader-png.dll\""), std::string::npos);
}

TEST(RebasePixbufLoadersCacheTest, KeepsWindowsLineEndings)
{
    const std::string cache = "# header\r\n\"lib\\\\loaders\\\\libpixbufloader-png.dll\"\r\n\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\r\n";

    const auto rebased = rebasePixbufLoadersCache(cache, fs::path("/app"));

    EXPECT_EQ(rebased, "# header\r\n\"/app/libpixbufloader-png.dll\"\r\n\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\r\n");
}

TEST(RebasePixbufLoadersCacheTest, AcceptsForwardSlashesAndUnixModules)
{
    const std::string cache = "\"/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.so\"\n";

    EXPECT_EQ(rebasePixbufLoadersCache(cache, fs::path("/app")), "\"/app/libpixbufloader-png.so\"\n");
}

TEST(RebasePixbufLoadersCacheTest, TextWithoutModuleLinesAndEmptyTextAreUnchanged)
{
    EXPECT_EQ(rebasePixbufLoadersCache("", fs::path("/app")), "");
    EXPECT_EQ(rebasePixbufLoadersCache("# only a comment", fs::path("/app")), "# only a comment");
    EXPECT_EQ(rebasePixbufLoadersCache("\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\n", fs::path("/app")), "\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\n");
}

TEST_F(BundledRuntimeTest, FolderWithoutBundledDataChangesNothing)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;

    configureBundledRuntime(bundle.path(), cache.path());

    EXPECT_EQ(std::getenv("GSETTINGS_SCHEMA_DIR"), nullptr);
    EXPECT_EQ(std::getenv("XDG_DATA_DIRS"), nullptr);
    EXPECT_EQ(std::getenv("FONTCONFIG_PATH"), nullptr);
    EXPECT_EQ(std::getenv("GDK_PIXBUF_MODULE_FILE"), nullptr);
    EXPECT_TRUE(fs::is_empty(cache.path()));
}

TEST_F(BundledRuntimeTest, PointsGlibAtTheBundledSchemasAndData)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;
    createFile(bundle.path() / "share/glib-2.0/schemas/gschemas.compiled");
    createFile(bundle.path() / "etc/fonts/fonts.conf");

    configureBundledRuntime(bundle.path(), cache.path());

    ASSERT_NE(std::getenv("GSETTINGS_SCHEMA_DIR"), nullptr);
    EXPECT_EQ(std::string(std::getenv("GSETTINGS_SCHEMA_DIR")), (bundle.path() / "share/glib-2.0/schemas").string());
    ASSERT_NE(std::getenv("XDG_DATA_DIRS"), nullptr);
    EXPECT_EQ(std::string(std::getenv("XDG_DATA_DIRS")), (bundle.path() / "share").string());
    ASSERT_NE(std::getenv("FONTCONFIG_PATH"), nullptr);
    EXPECT_EQ(std::string(std::getenv("FONTCONFIG_PATH")), (bundle.path() / "etc/fonts").string());
}

TEST_F(BundledRuntimeTest, BundledShareDirectoryIsSearchedFirstAndOnlyOnce)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;
    createFile(bundle.path() / "share/icons/Adwaita/index.theme");
    setenv("XDG_DATA_DIRS", "/other/share", 1);

    configureBundledRuntime(bundle.path(), cache.path());
    configureBundledRuntime(bundle.path(), cache.path());

    EXPECT_EQ(std::string(std::getenv("XDG_DATA_DIRS")), (bundle.path() / "share").string() + ":/other/share");
}

TEST_F(BundledRuntimeTest, WritesARebasedLoadersCacheAndSelectsIt)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;
    createFile(bundle.path() / "lib/gdk-pixbuf-2.0/2.10.0/loaders.cache", SAMPLE_CACHE);
    createFile(bundle.path() / "lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.dll");

    configureBundledRuntime(bundle.path(), cache.path());

    ASSERT_NE(std::getenv("GDK_PIXBUF_MODULE_FILE"), nullptr);
    const fs::path cacheFile = std::getenv("GDK_PIXBUF_MODULE_FILE");
    EXPECT_EQ(cacheFile.parent_path(), cache.path());

    const auto loadersDirectory = (bundle.path() / "lib/gdk-pixbuf-2.0/2.10.0/loaders").string();
    EXPECT_NE(readAll(cacheFile).find("\"" + loadersDirectory + "/libpixbufloader-png.dll\""), std::string::npos);
    EXPECT_EQ(readAll(cacheFile).find("msys64"), std::string::npos);
}

TEST_F(BundledRuntimeTest, LoadersCacheOfAnotherVersionDirectoryIsFound)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;
    createFile(bundle.path() / "lib/gdk-pixbuf-2.0/2.11.0/loaders.cache", SAMPLE_CACHE);

    configureBundledRuntime(bundle.path(), cache.path());

    EXPECT_NE(std::getenv("GDK_PIXBUF_MODULE_FILE"), nullptr);
}
