#include "stapik/tutorial/Tutorial.hpp"

#include <gtest/gtest.h>

namespace
{
    using stapik::tutorial::Chapter;
    using stapik::tutorial::Tutorial;

    Chapter chapter(std::string id, std::vector<Chapter> children = {})
    {
        Chapter result;
        result.id = std::move(id);
        result.children = std::move(children);
        return result;
    }

    Tutorial sampleTutorial()
    {
        Tutorial tutorial;
        tutorial.chapters.push_back(chapter("start", { chapter("start/install"), chapter("start/first-run") }));
        tutorial.chapters.push_back(chapter("tips"));
        return tutorial;
    }

    std::vector<std::string> idsOf(const std::vector<const Chapter*>& chapters)
    {
        std::vector<std::string> ids;
        for (const auto* item : chapters)
            ids.push_back(item->id);
        return ids;
    }
}

TEST(TutorialTest, EmptyTutorialHasNothingToShow)
{
    const Tutorial tutorial;

    EXPECT_TRUE(tutorial.empty());
    EXPECT_EQ(tutorial.first(), nullptr);
    EXPECT_EQ(tutorial.find("start"), nullptr);
}

TEST(TutorialTest, ReadingOrderVisitsABookBeforeItsPages)
{
    EXPECT_EQ(idsOf(sampleTutorial().readingOrder()), (std::vector<std::string>{ "start", "start/install", "start/first-run", "tips" }));
}

TEST(TutorialTest, FindsChaptersAtAnyDepth)
{
    const auto tutorial = sampleTutorial();

    ASSERT_NE(tutorial.find("start/first-run"), nullptr);
    EXPECT_EQ(tutorial.find("start/first-run")->id, "start/first-run");
    EXPECT_EQ(tutorial.find("missing"), nullptr);
}

TEST(TutorialTest, FirstIsTheFirstTopLevelChapter)
{
    EXPECT_EQ(sampleTutorial().first()->id, "start");
}

TEST(TutorialTest, NextAndPreviousFollowTheReadingOrder)
{
    const auto tutorial = sampleTutorial();

    EXPECT_EQ(tutorial.next("start")->id, "start/install");
    EXPECT_EQ(tutorial.next("start/first-run")->id, "tips");
    EXPECT_EQ(tutorial.previous("tips")->id, "start/first-run");
    EXPECT_EQ(tutorial.previous("start/install")->id, "start");
}

TEST(TutorialTest, NoChapterBeforeTheFirstOrAfterTheLast)
{
    const auto tutorial = sampleTutorial();

    EXPECT_EQ(tutorial.previous("start"), nullptr);
    EXPECT_EQ(tutorial.next("tips"), nullptr);
}

TEST(TutorialTest, NeighboursOfAnUnknownChapterDoNotExist)
{
    const auto tutorial = sampleTutorial();

    EXPECT_EQ(tutorial.previous("missing"), nullptr);
    EXPECT_EQ(tutorial.next("missing"), nullptr);
}
