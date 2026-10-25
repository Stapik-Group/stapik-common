#include "stapik/markdown/MarkdownDocument.hpp"
#include "stapik/tutorial/TutorialLoader.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
    namespace fs = std::filesystem;
    using stapik::tutorial::findLanguageDirectory;
    using stapik::tutorial::loadTutorial;

    class TutorialLoaderTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_root = stapik::test::createTemporaryDirectory("stapik-tutorial");
        }

        void TearDown() override
        {
            std::error_code errorCode;
            fs::remove_all(m_root, errorCode);
        }

        void write(const std::string& relativePath, const std::string& text) const
        {
            const auto path = m_root / relativePath;
            fs::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary) << text;
        }

        fs::path m_root;
    };
}

TEST_F(TutorialLoaderTest, MissingDirectoryGivesAnEmptyTutorial)
{
    EXPECT_TRUE(loadTutorial(m_root / "missing", "en").empty());
}

TEST_F(TutorialLoaderTest, LanguageDirectoryFollowsThePrimaryLanguage)
{
    write("pl/01-start.md", "# Start");
    write("en/01-start.md", "# Start");

    EXPECT_EQ(findLanguageDirectory(m_root, "pl_PL.UTF-8"), m_root / "pl");
}

TEST_F(TutorialLoaderTest, UntranslatedLanguageFallsBackToEnglish)
{
    write("en/01-start.md", "# Start");

    EXPECT_EQ(findLanguageDirectory(m_root, "de"), m_root / "en");
}

TEST_F(TutorialLoaderTest, NoLanguageDirectoryAtAll)
{
    write("fr/01-start.md", "# Start");

    EXPECT_FALSE(findLanguageDirectory(m_root, "de").has_value());
}

TEST_F(TutorialLoaderTest, PagesAreOrderedByFileName)
{
    write("en/20-second.md", "# Second");
    write("en/10-first.md", "# First");
    write("en/30-third.md", "# Third");

    const auto tutorial = loadTutorial(m_root, "en");

    ASSERT_EQ(tutorial.chapters.size(), 3U);
    EXPECT_EQ(tutorial.chapters[0].title, "First");
    EXPECT_EQ(tutorial.chapters[1].title, "Second");
    EXPECT_EQ(tutorial.chapters[2].title, "Third");
}

TEST_F(TutorialLoaderTest, IdDropsTheOrderPrefixAndTheExtension)
{
    write("en/01-getting-started.md", "# Getting started");

    const auto tutorial = loadTutorial(m_root, "en");

    ASSERT_EQ(tutorial.chapters.size(), 1U);
    EXPECT_EQ(tutorial.chapters[0].id, "getting-started");
}

TEST_F(TutorialLoaderTest, TitleIsTheFirstHeading)
{
    write("en/01-intro.md", "Some text first\n\n## The real *title*\n\n# Later");

    EXPECT_EQ(loadTutorial(m_root, "en").chapters[0].title, "The real title");
}

TEST_F(TutorialLoaderTest, TitleFallsBackToTheFileName)
{
    write("en/01-first_steps.md", "No heading in this page.");

    EXPECT_EQ(loadTutorial(m_root, "en").chapters[0].title, "First steps");
}

TEST_F(TutorialLoaderTest, PageKeepsItsContentAndDirectory)
{
    write("en/01-intro.md", "# Intro\n\nHello.");

    const auto tutorial = loadTutorial(m_root, "en");

    EXPECT_EQ(tutorial.chapters[0].content.size(), 2U);
    EXPECT_EQ(tutorial.chapters[0].directory, m_root / "en");
}

TEST_F(TutorialLoaderTest, DirectoryIsABookWithItsOwnPage)
{
    write("en/01-basics/index.md", "# Basics\n\nOverview.");
    write("en/01-basics/01-open.md", "# Opening");
    write("en/01-basics/02-save.md", "# Saving");

    const auto tutorial = loadTutorial(m_root, "en");

    ASSERT_EQ(tutorial.chapters.size(), 1U);
    const auto& book = tutorial.chapters[0];
    EXPECT_EQ(book.id, "basics");
    EXPECT_EQ(book.title, "Basics");
    ASSERT_EQ(book.children.size(), 2U);
    EXPECT_EQ(book.children[0].id, "basics/open");
    EXPECT_EQ(book.children[1].id, "basics/save");
    EXPECT_EQ(book.content.size(), 2U);
}

TEST_F(TutorialLoaderTest, BookWithoutAnIndexGetsAContentsPage)
{
    write("en/01-basics/01-open.md", "# Opening");
    write("en/01-basics/02-save.md", "# Saving");

    const auto tutorial = loadTutorial(m_root, "en");
    const auto& book = tutorial.chapters[0];

    EXPECT_EQ(book.title, "Basics");
    ASSERT_EQ(book.content.size(), 3U);
    EXPECT_EQ(book.content[0].kind, stapik::markdown::BlockKind::Heading);
    EXPECT_EQ(book.content[1].kind, stapik::markdown::BlockKind::ListItem);
    EXPECT_EQ(book.content[1].spans[0].text, "Opening");
    EXPECT_EQ(book.content[1].spans[0].link, "#basics/open");
    EXPECT_EQ(book.content[2].spans[0].link, "#basics/save");
}

TEST_F(TutorialLoaderTest, DirectoriesWithoutPagesAreIgnored)
{
    write("en/images/screenshot.png", "not a page");
    write("en/01-intro.md", "# Intro");

    const auto tutorial = loadTutorial(m_root, "en");

    ASSERT_EQ(tutorial.chapters.size(), 1U);
    EXPECT_EQ(tutorial.chapters[0].id, "intro");
}

TEST_F(TutorialLoaderTest, OnlyMarkdownPagesAreLoaded)
{
    write("en/01-intro.md", "# Intro");
    write("en/notes.txt", "ignored");
    write("en/.hidden.md", "# Hidden");

    EXPECT_EQ(loadTutorial(m_root, "en").chapters.size(), 1U);
}

TEST_F(TutorialLoaderTest, EmptyPagesAreSkipped)
{
    write("en/01-empty.md", "\n\n");
    write("en/02-intro.md", "# Intro");

    const auto tutorial = loadTutorial(m_root, "en");

    ASSERT_EQ(tutorial.chapters.size(), 1U);
    EXPECT_EQ(tutorial.chapters[0].id, "intro");
}

TEST_F(TutorialLoaderTest, ByteOrderMarkIsNotPartOfThePage)
{
    write("en/01-intro.md", "\xEF\xBB\xBF# Intro");

    const auto tutorial = loadTutorial(m_root, "en");

    ASSERT_EQ(tutorial.chapters[0].content.size(), 1U);
    EXPECT_EQ(tutorial.chapters[0].content[0].kind, stapik::markdown::BlockKind::Heading);
}

TEST_F(TutorialLoaderTest, ChaptersOfSeveralLanguagesAreIndependent)
{
    write("en/01-intro.md", "# Introduction");
    write("pl/01-intro.md", "# Wprowadzenie");

    EXPECT_EQ(loadTutorial(m_root, "pl").chapters[0].title, "Wprowadzenie");
    EXPECT_EQ(loadTutorial(m_root, "en").chapters[0].title, "Introduction");
}
