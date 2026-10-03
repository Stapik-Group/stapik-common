#include "stapik/domain/Category.hpp"
#include "stapik/domain/CategoryJson.hpp"

#include "stapik/storage/AppPaths.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace stapik::domain;

namespace
{
    std::string readFile(const std::filesystem::path& file)
    {
        std::ifstream stream(file);
        std::ostringstream content;
        content << stream.rdbuf();
        return content.str();
    }
}

TEST(CategoryColorTest, IdsRoundTripForEveryColor)
{
    std::set<std::string> seenIds;
    for (const auto color : allCategoryColors())
    {
        const auto colorId = std::string(categoryColorId(color));
        EXPECT_FALSE(colorId.empty());
        EXPECT_TRUE(seenIds.insert(colorId).second) << "duplicate id " << colorId;
        EXPECT_EQ(categoryColorFromId(colorId), color);
    }
}

TEST(CategoryColorTest, UnknownIdIsRejected)
{
    EXPECT_FALSE(categoryColorFromId("").has_value());
    EXPECT_FALSE(categoryColorFromId("Red").has_value());
    EXPECT_FALSE(categoryColorFromId("magenta").has_value());
}

TEST(CategoryColorTest, NameKeyUsesColorId)
{
    EXPECT_EQ(categoryColorNameKey(CategoryColor::Teal), "category.color.teal");
}

TEST(CategoryColorTest, CssClassUsesStapikPrefix)
{
    EXPECT_EQ(categoryColorCssClass(CategoryColor::Red), "stapik-category-red");
    EXPECT_EQ(categoryColorCssClass(CategoryColor::Gray), "stapik-category-gray");
}

TEST(CategoryColorTest, EveryCommonThemeStylesEveryColor)
{
    const auto themesDir = AppPaths::commonResourcesDir() / "themes";
    ASSERT_TRUE(std::filesystem::exists(themesDir)) << themesDir;

    int themeCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(themesDir))
    {
        if (entry.path().extension() != ".css")
            continue;

        ++themeCount;
        const auto css = readFile(entry.path());
        for (const auto color : allCategoryColors())
        {
            const auto selector = "." + categoryColorCssClass(color);
            EXPECT_NE(css.find(selector), std::string::npos) << entry.path().filename() << " lacks " << selector;
        }
    }

    EXPECT_GT(themeCount, 0);
}

TEST(CategoryTest, JsonRoundTrips)
{
    const Category category{"abc123", "Groceries", CategoryColor::Teal};

    const nlohmann::json json = category;
    EXPECT_EQ(json.at("color"), "teal");
    EXPECT_EQ(json.get<Category>(), category);
}

TEST(CategoryTest, JsonWithUnknownColorIsRejected)
{
    const auto json = nlohmann::json::parse(R"({"id": "abc", "name": "Rent", "color": "magenta"})");
    EXPECT_THROW((void)json.get<Category>(), std::invalid_argument);
}

TEST(CategoryTest, JsonWithMissingFieldIsRejected)
{
    const auto json = nlohmann::json::parse(R"({"id": "abc", "color": "red"})");
    EXPECT_THROW((void)json.get<Category>(), nlohmann::json::out_of_range);
}

TEST(CategoryTest, CssClassFollowsColor)
{
    const Category category{"abc", "Rent", CategoryColor::Brown};
    EXPECT_EQ(category.cssClass(), "stapik-category-brown");
}
