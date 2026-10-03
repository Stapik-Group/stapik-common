#pragma once

#include "CategoryColor.hpp"

#include <string>

namespace stapik::domain
{
    struct Category
    {
        std::string id;
        std::string name;
        CategoryColor color = CategoryColor::Gray;

        bool operator==(const Category&) const = default;

        [[nodiscard]] std::string cssClass() const
        {
            return categoryColorCssClass(color);
        }
    };
}
