#pragma once

#include <string>
#include <vector>

namespace stapik::markdown
{
    struct Span
    {
        std::string text;
        bool bold = false;
        bool italic = false;
        bool code = false;
        std::string link;

        bool operator==(const Span&) const = default;
    };

    enum class BlockKind
    {
        Heading,
        Paragraph,
        ListItem,
        Quote,
        CodeBlock,
        Image,
        Rule
    };

    struct Block
    {
        BlockKind kind = BlockKind::Paragraph;
        int headingLevel = 0;
        int listDepth = 0;
        int listNumber = 0;
        std::vector<Span> spans;
        std::string imagePath;

        [[nodiscard]] bool isNumberedListItem() const
        {
            return kind == BlockKind::ListItem && listNumber > 0;
        }

        bool operator==(const Block&) const = default;
    };

    using Document = std::vector<Block>;

    [[nodiscard]] inline std::string plainText(const std::vector<Span>& spans)
    {
        std::string text;
        for (const auto& span : spans)
            text += span.text;
        return text;
    }
}
