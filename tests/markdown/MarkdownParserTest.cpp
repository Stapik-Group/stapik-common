#include "stapik/markdown/MarkdownParser.hpp"

#include <gtest/gtest.h>

namespace
{
    using stapik::markdown::Block;
    using stapik::markdown::BlockKind;
    using stapik::markdown::parseInline;
    using stapik::markdown::parseMarkdown;
    using stapik::markdown::plainText;
    using stapik::markdown::Span;

    Span styled(std::string text, const bool bold, const bool italic, const bool code, std::string link)
    {
        Span span;
        span.text = std::move(text);
        span.bold = bold;
        span.italic = italic;
        span.code = code;
        span.link = std::move(link);
        return span;
    }

    Span plain(std::string text) { return styled(std::move(text), false, false, false, {}); }
    Span bold(std::string text) { return styled(std::move(text), true, false, false, {}); }
    Span italic(std::string text) { return styled(std::move(text), false, true, false, {}); }
    Span boldItalic(std::string text) { return styled(std::move(text), true, true, false, {}); }
    Span code(std::string text) { return styled(std::move(text), false, false, true, {}); }
    Span linked(std::string text, std::string target) { return styled(std::move(text), false, false, false, std::move(target)); }
    Span boldLinked(std::string text, std::string target) { return styled(std::move(text), true, false, false, std::move(target)); }
}

TEST(MarkdownParserTest, EmptyTextHasNoBlocks)
{
    EXPECT_TRUE(parseMarkdown("").empty());
    EXPECT_TRUE(parseMarkdown("\n  \n\n").empty());
}

TEST(MarkdownParserTest, HeadingKeepsItsLevelAndText)
{
    const auto document = parseMarkdown("# Welcome\n\n### Details");

    ASSERT_EQ(document.size(), 2U);
    EXPECT_EQ(document[0].kind, BlockKind::Heading);
    EXPECT_EQ(document[0].headingLevel, 1);
    EXPECT_EQ(plainText(document[0].spans), "Welcome");
    EXPECT_EQ(document[1].headingLevel, 3);
    EXPECT_EQ(plainText(document[1].spans), "Details");
}

TEST(MarkdownParserTest, HashesWithoutASpaceAreNotAHeading)
{
    const auto document = parseMarkdown("#hashtag\n\n####### seven");

    ASSERT_EQ(document.size(), 2U);
    EXPECT_EQ(document[0].kind, BlockKind::Paragraph);
    EXPECT_EQ(document[1].kind, BlockKind::Paragraph);
}

TEST(MarkdownParserTest, ConsecutiveLinesFormOneParagraph)
{
    const auto document = parseMarkdown("first line\nsecond line\n\nnext paragraph");

    ASSERT_EQ(document.size(), 2U);
    EXPECT_EQ(plainText(document[0].spans), "first line second line");
    EXPECT_EQ(plainText(document[1].spans), "next paragraph");
}

TEST(MarkdownParserTest, WindowsLineEndingsAreAccepted)
{
    const auto document = parseMarkdown("# Title\r\n\r\ntext\r\nmore\r\n");

    ASSERT_EQ(document.size(), 2U);
    EXPECT_EQ(plainText(document[0].spans), "Title");
    EXPECT_EQ(plainText(document[1].spans), "text more");
}

TEST(MarkdownParserTest, BulletListItemsAreSeparateBlocks)
{
    const auto document = parseMarkdown("- one\n- two\n* three");

    ASSERT_EQ(document.size(), 3U);
    for (const auto& item : document)
    {
        EXPECT_EQ(item.kind, BlockKind::ListItem);
        EXPECT_FALSE(item.isNumberedListItem());
    }
    EXPECT_EQ(plainText(document[2].spans), "three");
}

TEST(MarkdownParserTest, NumberedListItemsKeepTheirNumbers)
{
    const auto document = parseMarkdown("1. open\n2. save\n10. quit");

    ASSERT_EQ(document.size(), 3U);
    EXPECT_TRUE(document[0].isNumberedListItem());
    EXPECT_EQ(document[1].listNumber, 2);
    EXPECT_EQ(document[2].listNumber, 10);
    EXPECT_EQ(plainText(document[2].spans), "quit");
}

TEST(MarkdownParserTest, NestedListItemsGetADepth)
{
    const auto document = parseMarkdown("- parent\n  - child\n    - grandchild");

    ASSERT_EQ(document.size(), 3U);
    EXPECT_EQ(document[0].listDepth, 0);
    EXPECT_EQ(document[1].listDepth, 1);
    EXPECT_EQ(document[2].listDepth, 2);
}

TEST(MarkdownParserTest, ListItemContinuesOnTheNextLine)
{
    const auto document = parseMarkdown("- a long item\n  that goes on\n- next");

    ASSERT_EQ(document.size(), 2U);
    EXPECT_EQ(plainText(document[0].spans), "a long item that goes on");
}

TEST(MarkdownParserTest, QuoteLinesAreJoined)
{
    const auto document = parseMarkdown("> Tip: press F1\n> at any time");

    ASSERT_EQ(document.size(), 1U);
    EXPECT_EQ(document[0].kind, BlockKind::Quote);
    EXPECT_EQ(plainText(document[0].spans), "Tip: press F1 at any time");
}

TEST(MarkdownParserTest, FencedCodeKeepsLinesAndIgnoresMarkup)
{
    const auto document = parseMarkdown("```\n# not a heading\n  **kept**\n```\nafter");

    ASSERT_EQ(document.size(), 2U);
    EXPECT_EQ(document[0].kind, BlockKind::CodeBlock);
    EXPECT_EQ(plainText(document[0].spans), "# not a heading\n  **kept**");
    EXPECT_EQ(plainText(document[1].spans), "after");
}

TEST(MarkdownParserTest, UnclosedCodeFenceRunsToTheEnd)
{
    const auto document = parseMarkdown("```\nline one\nline two");

    ASSERT_EQ(document.size(), 1U);
    EXPECT_EQ(plainText(document[0].spans), "line one\nline two");
}

TEST(MarkdownParserTest, ImageOnItsOwnLineIsABlock)
{
    const auto document = parseMarkdown("![Main window](images/main.png)");

    ASSERT_EQ(document.size(), 1U);
    EXPECT_EQ(document[0].kind, BlockKind::Image);
    EXPECT_EQ(document[0].imagePath, "images/main.png");
    EXPECT_EQ(plainText(document[0].spans), "Main window");
}

TEST(MarkdownParserTest, HorizontalRule)
{
    const auto document = parseMarkdown("above\n\n---\n\nbelow");

    ASSERT_EQ(document.size(), 3U);
    EXPECT_EQ(document[1].kind, BlockKind::Rule);
}

TEST(MarkdownParserTest, BlockStartInterruptsAParagraph)
{
    const auto document = parseMarkdown("text\n## Heading\n- item");

    ASSERT_EQ(document.size(), 3U);
    EXPECT_EQ(document[0].kind, BlockKind::Paragraph);
    EXPECT_EQ(document[1].kind, BlockKind::Heading);
    EXPECT_EQ(document[2].kind, BlockKind::ListItem);
}

TEST(MarkdownInlineTest, PlainTextIsOneSpan)
{
    EXPECT_EQ(parseInline("hello world"), (std::vector<Span>{ plain("hello world") }));
}

TEST(MarkdownInlineTest, BoldItalicAndCode)
{
    const auto spans = parseInline("a **bold** b *italic* c `code` d");

    ASSERT_EQ(spans.size(), 7U);
    EXPECT_EQ(spans[1], bold("bold"));
    EXPECT_EQ(spans[3], italic("italic"));
    EXPECT_EQ(spans[5], code("code"));
    EXPECT_EQ(plainText(spans), "a bold b italic c code d");
}

TEST(MarkdownInlineTest, ItalicInsideBold)
{
    const auto spans = parseInline("**very *important* text**");

    ASSERT_EQ(spans.size(), 3U);
    EXPECT_EQ(spans[0], bold("very "));
    EXPECT_EQ(spans[1], boldItalic("important"));
    EXPECT_EQ(spans[2], bold(" text"));
}

TEST(MarkdownInlineTest, BoldInsideItalic)
{
    const auto spans = parseInline("*a **b** c*");

    ASSERT_EQ(spans.size(), 3U);
    EXPECT_EQ(spans[1], boldItalic("b"));
}

TEST(MarkdownInlineTest, CodeSpanKeepsMarkupLiteral)
{
    const auto spans = parseInline("`**not bold**`");

    ASSERT_EQ(spans.size(), 1U);
    EXPECT_EQ(spans[0], code("**not bold**"));
}

TEST(MarkdownInlineTest, LinkCarriesItsTarget)
{
    const auto spans = parseInline("see [the docs](https://example.com/docs) now");

    ASSERT_EQ(spans.size(), 3U);
    EXPECT_EQ(spans[1], linked("the docs", "https://example.com/docs"));
}

TEST(MarkdownInlineTest, LinkTextCanBeStyled)
{
    const auto spans = parseInline("[**Start**](#getting-started)");

    ASSERT_EQ(spans.size(), 1U);
    EXPECT_EQ(spans[0], boldLinked("Start", "#getting-started"));
}

TEST(MarkdownInlineTest, UnmatchedMarkersStayAsText)
{
    EXPECT_EQ(plainText(parseInline("2 * 3 = 6, `open, [x](y")), "2 * 3 = 6, `open, [x](y");
    EXPECT_EQ(parseInline("a ** b").size(), 1U);
}

TEST(MarkdownInlineTest, BackslashEscapesMarkers)
{
    const auto spans = parseInline(R"(\*literal\* and \[brackets\])");

    ASSERT_EQ(spans.size(), 1U);
    EXPECT_EQ(spans[0].text, "*literal* and [brackets]");
}

TEST(MarkdownInlineTest, Utf8TextSurvives)
{
    const auto spans = parseInline("**Zażółć** gęślą jaźń");

    ASSERT_EQ(spans.size(), 2U);
    EXPECT_EQ(spans[0], bold("Zażółć"));
    EXPECT_EQ(spans[1].text, " gęślą jaźń");
}
