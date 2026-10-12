#pragma once

#include "MarkdownDocument.hpp"

#include <string_view>

namespace stapik::markdown
{
    [[nodiscard]] Document parseMarkdown(std::string_view markdown);
    [[nodiscard]] std::vector<Span> parseInline(std::string_view text);
}
