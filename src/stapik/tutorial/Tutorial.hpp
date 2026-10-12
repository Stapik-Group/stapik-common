#pragma once

#include "stapik/markdown/MarkdownDocument.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace stapik::tutorial
{
    struct Chapter
    {
        std::string id;
        std::string title;
        markdown::Document content;
        std::filesystem::path directory;
        std::vector<Chapter> children;
    };

    struct Tutorial
    {
        std::vector<Chapter> chapters;

        [[nodiscard]] bool empty() const;
        [[nodiscard]] const Chapter* find(std::string_view id) const;
        [[nodiscard]] const Chapter* first() const;
        [[nodiscard]] const Chapter* previous(std::string_view id) const;
        [[nodiscard]] const Chapter* next(std::string_view id) const;
        [[nodiscard]] std::vector<const Chapter*> readingOrder() const;
    };
}
