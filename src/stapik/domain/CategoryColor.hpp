#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace stapik::domain
{
    // Every value needs a matching `.stapik-category-<id>` rule in the common themes.
    enum class CategoryColor
    {
        Red,
        Orange,
        Yellow,
        Green,
        Teal,
        Blue,
        Purple,
        Pink,
        Brown,
        Gray
    };

    [[nodiscard]] std::span<const CategoryColor> allCategoryColors();
    [[nodiscard]] std::string_view categoryColorId(CategoryColor color);
    [[nodiscard]] std::optional<CategoryColor> categoryColorFromId(std::string_view colorId);
    [[nodiscard]] std::string categoryColorNameKey(CategoryColor color);
    [[nodiscard]] std::string categoryColorCssClass(CategoryColor color);
}
