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

    // Stable identifier used in files and in the CSS class name, e.g. "red".
    [[nodiscard]] std::string_view categoryColorId(CategoryColor color);
    [[nodiscard]] std::optional<CategoryColor> categoryColorFromId(std::string_view colorId);

    // Locale key of the display name, e.g. "category.color.red" (provided by the common locales).
    [[nodiscard]] std::string categoryColorNameKey(CategoryColor color);

    // E.g. "stapik-category-red".
    [[nodiscard]] std::string categoryColorCssClass(CategoryColor color);
}
