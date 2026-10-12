#include "stapik/ui/dialog/FileDrop.hpp"

#include <gtest/gtest.h>

namespace
{
    namespace fs = std::filesystem;
    using stapik::ui::FileDropOptions;
    using stapik::ui::filterDroppedFiles;
}

TEST(FileDropTest, NoExtensionsAcceptsEveryFile)
{
    const std::vector<fs::path> paths{ "/a/one.csv", "/a/two", "/a/three.PDF" };

    EXPECT_EQ(filterDroppedFiles(paths, {}), paths);
}

TEST(FileDropTest, KeepsOnlyListedExtensionsWithoutRegardToCase)
{
    const FileDropOptions options{ .extensions = { ".csv", ".PDF" } };

    const auto accepted = filterDroppedFiles({ "/a/one.CSV", "/a/two.txt", "/a/three.pdf", "/a/four" }, options);

    EXPECT_EQ(accepted, (std::vector<fs::path>{ "/a/one.CSV", "/a/three.pdf" }));
}

TEST(FileDropTest, SingleFileModeKeepsTheFirstAcceptedFile)
{
    const FileDropOptions options{ .extensions = { ".csv" }, .allowMultiple = false };

    const auto accepted = filterDroppedFiles({ "/a/skip.txt", "/a/one.csv", "/a/two.csv" }, options);

    EXPECT_EQ(accepted, std::vector<fs::path>{ "/a/one.csv" });
}

TEST(FileDropTest, NothingAcceptableYieldsNothing)
{
    const FileDropOptions options{ .extensions = { ".csv" } };

    EXPECT_TRUE(filterDroppedFiles({ "/a/one.txt" }, options).empty());
}
