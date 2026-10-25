#include "stapik/app/BundledRuntime.hpp"

#include "support/Environment.hpp"
#include "support/ScopedEnvironment.hpp"
#include "support/TemporaryDirectory.hpp"

#include <glib.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using stapik::app::configureBundledRuntime;
    using stapik::app::rebasePixbufLoadersCache;
    using stapik::test::TemporaryDirectory;

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

#ifdef _WIN32
    constexpr auto ESCAPED_SEPARATOR = "\\\\";
#else
    constexpr auto ESCAPED_SEPARATOR = "/";
#endif

    std::string moduleLine(const std::string& directory, const std::string& fileName)
    {
        return "\"" + directory + ESCAPED_SEPARATOR + fileName + "\"";
    }

    std::vector<fs::path> modulePathsIn(const std::string& cacheText)
    {
        std::vector<fs::path> modules;
        std::istringstream stream(cacheText);
        for (std::string line; std::getline(stream, line);)
        {
            if (!line.starts_with('"') || !line.ends_with(".dll\""))
                continue;

            std::string unescaped;
            for (std::size_t index = 1; index + 1 < line.size(); ++index)
            {
                if (line[index] == '\\' && index + 2 < line.size())
                    ++index;
                unescaped += line[index];
            }

            modules.emplace_back(unescaped);
        }

        return modules;
    }

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

    class BundledRuntimeTest : public testing::Test
    {
    protected:
        stapik::test::ScopedEnvironment m_schemaDir{ "GSETTINGS_SCHEMA_DIR" };
        stapik::test::ScopedEnvironment m_dataDirs{ "XDG_DATA_DIRS" };
        stapik::test::ScopedEnvironment m_fontconfig{ "FONTCONFIG_PATH" };
        stapik::test::ScopedEnvironment m_pixbufFile{ "GDK_PIXBUF_MODULE_FILE" };

        void SetUp() override
        {
            stapik::test::unsetEnvironment("GSETTINGS_SCHEMA_DIR");
            stapik::test::unsetEnvironment("XDG_DATA_DIRS");
            stapik::test::unsetEnvironment("FONTCONFIG_PATH");
            stapik::test::unsetEnvironment("GDK_PIXBUF_MODULE_FILE");
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

    EXPECT_EQ(lines[3], moduleLine("/app/lib/loaders", "libpixbufloader-png.dll"));
    EXPECT_EQ(lines[9], moduleLine("/app/lib/loaders", "libpixbufloader-svg.dll"));
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

    EXPECT_NE(rebased.find(moduleLine("C:\\\\Users\\\\Zosia\\\\app\\\\loaders", "libpixbufloader-png.dll")), std::string::npos);
}

TEST(RebasePixbufLoadersCacheTest, KeepsWindowsLineEndings)
{
    const std::string cache = "# header\r\n\"lib\\\\loaders\\\\libpixbufloader-png.dll\"\r\n\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\r\n";

    const auto rebased = rebasePixbufLoadersCache(cache, fs::path("/app"));

    EXPECT_EQ(rebased, "# header\r\n" + moduleLine("/app", "libpixbufloader-png.dll") + "\r\n\"png\" 5 \"gdk-pixbuf\" \"PNG\" \"LGPL\"\r\n");
}

TEST(RebasePixbufLoadersCacheTest, AcceptsForwardSlashesAndUnixModules)
{
    const std::string cache = "\"/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.so\"\n";

    EXPECT_EQ(rebasePixbufLoadersCache(cache, fs::path("/app")), moduleLine("/app", "libpixbufloader-png.so") + "\n");
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
    EXPECT_EQ(fs::path(std::getenv("GSETTINGS_SCHEMA_DIR")), bundle.path() / "share/glib-2.0/schemas");
    ASSERT_NE(std::getenv("XDG_DATA_DIRS"), nullptr);
    EXPECT_EQ(fs::path(std::getenv("XDG_DATA_DIRS")), bundle.path() / "share");
    ASSERT_NE(std::getenv("FONTCONFIG_PATH"), nullptr);
    EXPECT_EQ(fs::path(std::getenv("FONTCONFIG_PATH")), bundle.path() / "etc/fonts");
}

TEST_F(BundledRuntimeTest, BundledShareDirectoryIsSearchedFirstAndOnlyOnce)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;
    createFile(bundle.path() / "share/icons/Adwaita/index.theme");
    stapik::test::setEnvironment("XDG_DATA_DIRS", "/other/share");

    configureBundledRuntime(bundle.path(), cache.path());
    configureBundledRuntime(bundle.path(), cache.path());

    EXPECT_EQ(std::string(std::getenv("XDG_DATA_DIRS")), (bundle.path() / "share").string() + G_SEARCHPATH_SEPARATOR_S + "/other/share");
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

    const auto expectedModule = (bundle.path() / "lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.dll").lexically_normal();
    const auto modules = modulePathsIn(readAll(cacheFile));
    EXPECT_TRUE(std::ranges::any_of(modules, [&](const fs::path& module)
    {
        return module.lexically_normal() == expectedModule;
    }));
    EXPECT_EQ(readAll(cacheFile).find("ucrt64"), std::string::npos);
}

TEST_F(BundledRuntimeTest, LoadersCacheOfAnotherVersionDirectoryIsFound)
{
    const TemporaryDirectory bundle;
    const TemporaryDirectory cache;
    createFile(bundle.path() / "lib/gdk-pixbuf-2.0/2.11.0/loaders.cache", SAMPLE_CACHE);

    configureBundledRuntime(bundle.path(), cache.path());

    EXPECT_NE(std::getenv("GDK_PIXBUF_MODULE_FILE"), nullptr);
}
