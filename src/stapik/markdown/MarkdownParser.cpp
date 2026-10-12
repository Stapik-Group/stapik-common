#include "MarkdownParser.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <utility>

namespace stapik::markdown
{
    namespace
    {
        constexpr int MAX_HEADING_LEVEL = 6;
        constexpr int MAX_BLOCK_INDENT = 3;
        constexpr int SPACES_PER_LIST_DEPTH = 2;
        constexpr int SPACES_PER_TAB = 4;
        constexpr std::string_view CODE_FENCE = "```";
        constexpr std::string_view BOLD_MARKER = "**";
        constexpr std::string_view WHITESPACE = " \t\r\n";

        struct InlineStyle
        {
            bool bold = false;
            bool italic = false;
        };

        Span makeSpan(std::string text, const InlineStyle style, const std::string &link)
        {
            Span span;
            span.text = std::move(text);
            span.bold = style.bold;
            span.italic = style.italic;
            span.link = link;
            return span;
        }

        Block makeBlock(const BlockKind kind, std::vector<Span> spans = {})
        {
            Block block;
            block.kind = kind;
            block.spans = std::move(spans);
            return block;
        }

        std::string_view trimmed(const std::string_view text)
        {
            const auto first = text.find_first_not_of(WHITESPACE);
            if (first == std::string_view::npos)
                return {};

            const auto last = text.find_last_not_of(WHITESPACE);
            return text.substr(first, last - first + 1);
        }

        std::string_view withoutTrailingWhitespace(const std::string_view text)
        {
            const auto last = text.find_last_not_of(WHITESPACE);
            return last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
        }

        int indentOf(const std::string_view line)
        {
            int indent = 0;
            for (const char character: line)
            {
                if (character == ' ')
                    ++indent;
                else if (character == '\t')
                    indent += SPACES_PER_TAB;
                else
                    break;
            }
            return indent;
        }

        std::vector<std::string_view> splitLines(std::string_view markdown)
        {
            std::vector<std::string_view> lines;
            while (!markdown.empty())
            {
                const auto end = markdown.find('\n');
                lines.push_back(withoutTrailingWhitespace(markdown.substr(0, end)));
                markdown.remove_prefix(end == std::string_view::npos ? markdown.size() : end + 1);
            }
            return lines;
        }

        bool isPunctuation(const char character)
        {
            return std::ispunct(static_cast<unsigned char>(character)) != 0;
        }

        std::size_t findClosingEmphasis(const std::string_view text, std::size_t from)
        {
            while (from < text.size())
            {
                if (text[from] == '\\')
                    from += 2;
                else if (text.substr(from).starts_with(BOLD_MARKER))
                    from += BOLD_MARKER.size();
                else if (text[from] == '*')
                    return from;
                else
                    ++from;
            }
            return std::string_view::npos;
        }

        void parseInlineInto(std::vector<Span> &spans, std::string_view text, const InlineStyle style,
                             const std::string &link)
        {
            std::string buffer;

            const auto flush = [&]
            {
                if (buffer.empty())
                    return;
                spans.push_back(makeSpan(std::move(buffer), style, link));
                buffer.clear();
            };

            for (std::size_t position = 0; position < text.size(); ++position)
            {
                const char current = text[position];

                if (current == '\\' && position + 1 < text.size() && isPunctuation(text[position + 1]))
                {
                    buffer += text[++position];
                    continue;
                }

                if (current == '`')
                {
                    if (const auto closing = text.find('`', position + 1);
                        closing != std::string_view::npos && closing > position + 1)
                    {
                        flush();
                        auto code = makeSpan(std::string(text.substr(position + 1, closing - position - 1)), style,
                                             link);
                        code.code = true;
                        spans.push_back(std::move(code));
                        position = closing;
                        continue;
                    }
                }

                if (text.substr(position).starts_with(BOLD_MARKER))
                {
                    if (const auto closing = text.find(BOLD_MARKER, position + BOLD_MARKER.size());
                        closing != std::string_view::npos && closing > position + BOLD_MARKER.size())
                    {
                        flush();
                        const auto inner = text.substr(position + BOLD_MARKER.size(),
                                                       closing - position - BOLD_MARKER.size());
                        parseInlineInto(spans, inner, {.bold = true, .italic = style.italic}, link);
                        position = closing + BOLD_MARKER.size() - 1;
                        continue;
                    }
                }

                if (current == '*')
                {
                    if (const auto closing = findClosingEmphasis(text, position + 1);
                        closing != std::string_view::npos && closing > position + 1)
                    {
                        flush();
                        parseInlineInto(spans, text.substr(position + 1, closing - position - 1),
                                        {.bold = style.bold, .italic = true}, link);
                        position = closing;
                        continue;
                    }
                }

                if (current == '[')
                {
                    const auto labelEnd = text.find("](", position + 1);
                    if (const auto targetEnd = labelEnd == std::string_view::npos
                                                   ? labelEnd
                                                   : text.find(')', labelEnd + 2);
                        targetEnd != std::string_view::npos && labelEnd > position + 1)
                    {
                        flush();
                        const std::string target(trimmed(text.substr(labelEnd + 2, targetEnd - labelEnd - 2)));
                        parseInlineInto(spans, text.substr(position + 1, labelEnd - position - 1), style, target);
                        position = targetEnd;
                        continue;
                    }
                }

                buffer += current;
            }

            flush();
        }

        struct ListMarker
        {
            int depth = 0;
            int number = 0;
            std::string_view text;
        };

        std::optional<ListMarker> parseListMarker(const std::string_view line)
        {
            const auto indent = indentOf(line);
            const auto content = trimmed(line);
            if (content.size() < 2)
                return std::nullopt;

            ListMarker marker;
            marker.depth = indent / SPACES_PER_LIST_DEPTH;

            if ((content[0] == '-' || content[0] == '*' || content[0] == '+') && content[1] == ' ')
            {
                marker.text = trimmed(content.substr(2));
                return marker;
            }

            const auto digitsEnd = content.find_first_not_of("0123456789");
            if (digitsEnd == 0 || digitsEnd == std::string_view::npos || digitsEnd > 9)
                return std::nullopt;
            if (content.substr(digitsEnd, 2) != ". ")
                return std::nullopt;

            marker.number = std::stoi(std::string(content.substr(0, digitsEnd)));
            marker.text = trimmed(content.substr(digitsEnd + 2));
            return marker;
        }

        std::optional<int> parseHeadingLevel(const std::string_view line, std::string_view &text)
        {
            if (indentOf(line) > MAX_BLOCK_INDENT)
                return std::nullopt;

            const auto content = trimmed(line);
            const auto markerEnd = content.find_first_not_of('#');
            if (markerEnd == 0 || markerEnd > MAX_HEADING_LEVEL)
                return std::nullopt;
            if (content[markerEnd] != ' ')
                return std::nullopt;

            text = trimmed(content.substr(markerEnd));
            return static_cast<int>(markerEnd);
        }

        bool isRule(const std::string_view line)
        {
            const auto content = trimmed(line);
            if (constexpr std::size_t MIN_RULE_LENGTH = 3;
                content.size() < MIN_RULE_LENGTH || (content[0] != '-' && content[0] != '*' && content[0] != '_'))
                return false;

            return std::ranges::all_of(content, [first = content[0]](const char character)
            {
                return character == first;
            });
        }

        bool parseImage(const std::string_view line, std::string_view &altText, std::string_view &path)
        {
            const auto content = trimmed(line);
            if (!content.starts_with("![") || !content.ends_with(')'))
                return false;

            const auto altEnd = content.find("](");
            if (altEnd == std::string_view::npos)
                return false;

            altText = content.substr(2, altEnd - 2);
            path = trimmed(content.substr(altEnd + 2, content.size() - altEnd - 3));
            return !path.empty() && path.find(')') == std::string_view::npos;
        }

        class BlockParser
        {
        public:
            Document parse(const std::string_view markdown)
            {
                for (const auto line: splitLines(markdown))
                    handle(line);

                flushPending();
                if (m_codeFence)
                    closeCodeFence();

                return std::move(m_document);
            }

        private:
            struct PendingText
            {
                BlockKind kind = BlockKind::Paragraph;
                int listDepth = 0;
                int listNumber = 0;
                std::string text;
            };

            void handle(const std::string_view line)
            {
                if (m_codeFence)
                {
                    handleCodeLine(line);
                    return;
                }

                const auto content = trimmed(line);
                if (content.empty())
                {
                    flushPending();
                    return;
                }

                std::string_view headingText;
                std::string_view imagePath;

                if (content.starts_with(CODE_FENCE))
                {
                    flushPending();
                    m_codeFence = std::string{};
                } else if (const auto level = parseHeadingLevel(line, headingText))
                {
                    flushPending();
                    auto heading = makeBlock(BlockKind::Heading, parseInline(headingText));
                    heading.headingLevel = *level;
                    m_document.push_back(std::move(heading));
                } else if (isRule(line))
                {
                    flushPending();
                    m_document.push_back(makeBlock(BlockKind::Rule));
                } else if (std::string_view altText; parseImage(line, altText, imagePath))
                {
                    flushPending();
                    auto image = makeBlock(BlockKind::Image, parseInline(altText));
                    image.imagePath = std::string(imagePath);
                    m_document.push_back(std::move(image));
                } else if (const auto marker = parseListMarker(line))
                {
                    flushPending();
                    m_pending = PendingText{
                        .kind = BlockKind::ListItem, .listDepth = marker->depth, .listNumber = marker->number,
                        .text = std::string(marker->text)
                    };
                } else if (content.starts_with('>'))
                {
                    continuePending(BlockKind::Quote, trimmed(content.substr(1)));
                } else
                {
                    using enum BlockKind;
                    continuePending(m_pending && m_pending->kind == ListItem ? ListItem : Paragraph, content);
                }
            }

            void handleCodeLine(const std::string_view line)
            {
                if (trimmed(line).starts_with(CODE_FENCE))
                {
                    closeCodeFence();
                    return;
                }

                if (!m_codeFence->empty())
                    *m_codeFence += '\n';
                *m_codeFence += line;
            }

            void closeCodeFence()
            {
                auto code = makeSpan(std::move(*m_codeFence), {}, {});
                code.code = true;
                m_document.push_back(makeBlock(BlockKind::CodeBlock, {std::move(code)}));
                m_codeFence.reset();
            }

            void continuePending(const BlockKind kind, const std::string_view text)
            {
                if (m_pending && m_pending->kind == kind)
                {
                    m_pending->text += ' ';
                    m_pending->text += text;
                    return;
                }

                flushPending();
                m_pending = PendingText{.kind = kind, .listDepth = 0, .listNumber = 0, .text = std::string(text)};
            }

            void flushPending()
            {
                if (!m_pending)
                    return;

                auto block = makeBlock(m_pending->kind, parseInline(m_pending->text));
                block.listDepth = m_pending->listDepth;
                block.listNumber = m_pending->listNumber;
                m_document.push_back(std::move(block));
                m_pending.reset();
            }

            Document m_document;
            std::optional<PendingText> m_pending;
            std::optional<std::string> m_codeFence;
        };
    }

    std::vector<Span> parseInline(const std::string_view text)
    {
        std::vector<Span> spans;
        parseInlineInto(spans, text, {}, {});
        return spans;
    }

    Document parseMarkdown(const std::string_view markdown)
    {
        return BlockParser().parse(markdown);
    }
}
