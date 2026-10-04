#include "stapik/domain/CurrencyCatalog.hpp"
#include "stapik/locale/LanguageRegistry.hpp"
#include "stapik/locale/LocalizationEngine.hpp"
#include "stapik/sync/SyncStatus.hpp"
#include "stapik/theme/ThemeRegistry.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// These tests guard the resources shipped with stapik-common (resources/locales, resources/themes).

namespace
{
    namespace fs = std::filesystem;

    const fs::path RESOURCES_DIR = STAPIK_COMMON_RESOURCES_DIR;

    const std::vector<std::string> LANGUAGES = { "de", "en", "pl" };
    const std::vector<std::string> THEMES = { "classic", "classic-pink", "dark", "modern", "neoclassic" };
    const std::vector<std::string> CATEGORY_COLORS = {
        "default", "red", "green", "blue", "yellow", "purple", "orange", "brown", "pink", "teal"
    };

    std::string readFile(const fs::path& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    nlohmann::json readLocale(const std::string& code)
    {
        std::ifstream file(RESOURCES_DIR / "locales" / (code + ".json"));
        return nlohmann::json::parse(file);
    }

    std::set<std::string> keysOf(const std::string& code)
    {
        const auto locale = readLocale(code);

        std::set<std::string> keys;
        for (const auto& [key, value] : locale.items())
            keys.insert(key);
        return keys;
    }
}

TEST(CommonResourcesTest, LocalesAreDiscoveredWithNativeDisplayNames)
{
    const stapik::locale::LanguageRegistry registry(RESOURCES_DIR / "locales");

    ASSERT_EQ(registry.languages().size(), LANGUAGES.size());
    EXPECT_EQ(registry.find("de")->displayName, "Deutsch");
    EXPECT_EQ(registry.find("en")->displayName, "English");
    EXPECT_EQ(registry.find("pl")->displayName, "Polski");
}

TEST(CommonResourcesTest, AllLanguagesHaveTheSameKeys)
{
    const auto english = keysOf("en");

    for (const auto& language : LANGUAGES)
    {
        const auto keys = keysOf(language);

        std::vector<std::string> missing;
        std::ranges::set_difference(english, keys, std::back_inserter(missing));
        std::vector<std::string> extra;
        std::ranges::set_difference(keys, english, std::back_inserter(extra));

        EXPECT_TRUE(missing.empty()) << language << " is missing e.g. " << (missing.empty() ? "" : missing.front());
        EXPECT_TRUE(extra.empty()) << language << " has an unknown key e.g. " << (extra.empty() ? "" : extra.front());
    }
}

TEST(CommonResourcesTest, AllTranslationsAreNonEmptyStrings)
{
    for (const auto& language : LANGUAGES)
    {
        const auto locale = readLocale(language);

        for (const auto& [key, value] : locale.items())
        {
            EXPECT_TRUE(value.is_string()) << language << ": " << key;

            if (value.is_string())
            {
                EXPECT_FALSE(value.get<std::string>().empty()) << language << ": " << key;
            }
        }
    }
}

TEST(CommonResourcesTest, EveryThemeHasADisplayNameInEveryLanguage)
{
    for (const auto& language : LANGUAGES)
    {
        const auto keys = keysOf(language);
        for (const auto& theme : THEMES)
            EXPECT_TRUE(keys.contains("theme." + theme)) << language << " lacks theme." << theme;
    }
}

TEST(CommonResourcesTest, ThemesAreDiscovered)
{
    stapik::theme::ThemeRegistry registry;
    registry.addDirectory(RESOURCES_DIR);

    ASSERT_EQ(registry.themes().size(), THEMES.size());
    for (const auto& theme : THEMES)
        EXPECT_TRUE(registry.contains(theme)) << theme;
}

TEST(CommonResourcesTest, ThemesDefineAllCategoryColors)
{
    for (const auto& theme : THEMES)
    {
        const auto css = readFile(RESOURCES_DIR / "themes" / (theme + ".css"));
        for (const auto& color : CATEGORY_COLORS)
            EXPECT_NE(css.find(".stapik-category-" + color), std::string::npos) << theme << " lacks " << color;
    }
}

TEST(CommonResourcesTest, ThemesHaveBalancedBracesAndNoApplicationSpecificSelectors)
{
    for (const auto& theme : THEMES)
    {
        const auto css = readFile(RESOURCES_DIR / "themes" / (theme + ".css"));

        EXPECT_EQ(std::ranges::count(css, '{'), std::ranges::count(css, '}')) << theme;
        EXPECT_EQ(css.find(".budget-"), std::string::npos) << theme;
    }
}

TEST(CommonResourcesTest, ThemesOnlyUseColorsTheyDefine)
{
    const std::regex defined(R"(@define-color\s+(\S+))");
    const std::regex used(R"(@(stapik_[A-Za-z0-9_]+))");

    for (const auto& theme : THEMES)
    {
        const auto css = readFile(RESOURCES_DIR / "themes" / (theme + ".css"));

        std::set<std::string> definedNames;
        for (std::sregex_iterator match(css.begin(), css.end(), defined), end; match != end; ++match)
            definedNames.insert((*match)[1]);

        for (std::sregex_iterator match(css.begin(), css.end(), used), end; match != end; ++match)
            EXPECT_TRUE(definedNames.contains((*match)[1])) << theme << " uses undefined @" << (*match)[1];
    }
}

TEST(CommonResourcesTest, EngineTranslatesFromTheCommonLocales)
{
    LocalizationEngine engine(RESOURCES_DIR / "locales");

    engine.setLanguage("pl");
    EXPECT_EQ(engine.translate("dialog.button.cancel"), "Anuluj");

    engine.setLanguage("de");
    EXPECT_EQ(engine.translate("dialog.button.cancel"), "Abbrechen");

    engine.setLanguage("en");
    EXPECT_EQ(engine.translate("dialog.button.cancel"), "Cancel");
}

TEST(CommonResourcesTest, ApplicationLocalesOverrideCommonOnes)
{
    auto pattern = (fs::temp_directory_path() / "stapik-common-override-XXXXXX").string();
    ASSERT_NE(mkdtemp(pattern.data()), nullptr);
    const fs::path appLocales = pattern;
    {
        std::ofstream file(appLocales / "en.json");
        file << R"({"dialog.button.cancel": "Never mind", "app.title": "My App"})";
    }

    LocalizationEngine engine(RESOURCES_DIR / "locales");
    engine.addLocalesDirectory(appLocales);
    engine.setLanguage("en");

    EXPECT_EQ(engine.translate("dialog.button.cancel"), "Never mind");
    EXPECT_EQ(engine.translate("dialog.button.ok"), "OK");
    EXPECT_EQ(engine.translate("app.title"), "My App");

    std::error_code errorCode;
    fs::remove_all(appLocales, errorCode);
}

TEST(CommonResourcesTest, EverySyncStatusHasATranslationInEveryLanguage)
{
    for (const auto& language : LANGUAGES)
    {
        const auto keys = keysOf(language);
        for (const auto status : stapik::sync::ALL_SYNC_STATUSES)
            EXPECT_TRUE(keys.contains(stapik::sync::syncStatusKey(status))) << language << " lacks " << stapik::sync::syncStatusKey(status);
    }
}

TEST(CommonResourcesTest, ThemesStyleEverySyncStatus)
{
    for (const auto& theme : THEMES)
    {
        const auto css = readFile(RESOURCES_DIR / "themes" / (theme + ".css"));
        for (const auto status : stapik::sync::ALL_SYNC_STATUSES)
            EXPECT_NE(css.find(std::string(".") + stapik::sync::syncStatusCssClass(status)), std::string::npos) << theme << " lacks " << stapik::sync::syncStatusCssClass(status);
    }
}

TEST(CommonResourcesTest, CurrencyCatalogLoadsAndEveryCurrencyHasAName)
{
    stapik::domain::CurrencyCatalog catalog;

    ASSERT_TRUE(catalog.addFromFile(RESOURCES_DIR / "currencies.json"));
    ASSERT_FALSE(catalog.currencies().empty());

    for (const auto& language : LANGUAGES)
    {
        const auto keys = keysOf(language);
        for (const auto& currency : catalog.currencies())
            EXPECT_TRUE(keys.contains(currency.nameKey())) << language << " lacks " << currency.nameKey();
    }
}
