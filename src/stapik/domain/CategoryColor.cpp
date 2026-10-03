#include "CategoryColor.hpp"

#include <algorithm>
#include <array>

namespace stapik::domain
{
    namespace
    {
        struct ColorEntry
        {
            CategoryColor color;
            std::string_view id;
        };

        constexpr std::array COLOR_ENTRIES{
            ColorEntry{CategoryColor::Red, "red"},
            ColorEntry{CategoryColor::Orange, "orange"},
            ColorEntry{CategoryColor::Yellow, "yellow"},
            ColorEntry{CategoryColor::Green, "green"},
            ColorEntry{CategoryColor::Teal, "teal"},
            ColorEntry{CategoryColor::Blue, "blue"},
            ColorEntry{CategoryColor::Purple, "purple"},
            ColorEntry{CategoryColor::Pink, "pink"},
            ColorEntry{CategoryColor::Brown, "brown"},
            ColorEntry{CategoryColor::Gray, "gray"}};

        constexpr std::array ALL_COLORS = []
        {
            std::array<CategoryColor, COLOR_ENTRIES.size()> colors{};
            for (std::size_t index = 0; index < COLOR_ENTRIES.size(); ++index)
                colors[index] = COLOR_ENTRIES[index].color;
            return colors;
        }();
    }

    std::span<const CategoryColor> allCategoryColors()
    {
        return ALL_COLORS;
    }

    std::string_view categoryColorId(const CategoryColor color)
    {
        const auto entry = std::ranges::find(COLOR_ENTRIES, color, &ColorEntry::color);
        return entry == COLOR_ENTRIES.end() ? std::string_view{} : entry->id;
    }

    std::optional<CategoryColor> categoryColorFromId(const std::string_view colorId)
    {
        const auto entry = std::ranges::find(COLOR_ENTRIES, colorId, &ColorEntry::id);
        if (entry == COLOR_ENTRIES.end())
            return std::nullopt;

        return entry->color;
    }

    std::string categoryColorNameKey(const CategoryColor color)
    {
        return "category.color." + std::string(categoryColorId(color));
    }

    std::string categoryColorCssClass(const CategoryColor color)
    {
        return "stapik-category-" + std::string(categoryColorId(color));
    }
}
