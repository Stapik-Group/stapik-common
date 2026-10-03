#include "stapik/domain/CategoryColor.hpp"
#include "stapik/domain/YearMonth.hpp"

#include "stapik/storage/AppPaths.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace stapik::domain;

namespace
{
    std::vector<std::filesystem::path> localeFiles()
    {
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(AppPaths::commonResourcesDir() / "locales"))
        {
            if (entry.path().extension() == ".json")
                files.push_back(entry.path());
        }

        return files;
    }

    nlohmann::json readJson(const std::filesystem::path& file)
    {
        std::ifstream stream(file);
        return nlohmann::json::parse(stream);
    }

    void expectNonEmptyString(const nlohmann::json& translations, const std::string& key, const std::string& fileName)
    {
        ASSERT_TRUE(translations.contains(key)) << fileName << " lacks " << key;
        ASSERT_TRUE(translations.at(key).is_string()) << fileName << ": " << key << " is not a string";
        EXPECT_FALSE(translations.at(key).get<std::string>().empty()) << fileName << ": " << key << " is empty";
    }
}

TEST(CommonLocaleKeysTest, EveryLanguageTranslatesEveryMonth)
{
    const auto files = localeFiles();
    ASSERT_FALSE(files.empty());

    for (const auto& file : files)
    {
        const auto translations = readJson(file);
        for (int month = 1; month <= MONTHS_PER_YEAR; ++month)
            expectNonEmptyString(translations, YearMonth(2026, month).monthNameKey(), file.filename().string());
    }
}

TEST(CommonLocaleKeysTest, EveryLanguageTranslatesEveryCategoryColor)
{
    const auto files = localeFiles();
    ASSERT_FALSE(files.empty());

    for (const auto& file : files)
    {
        const auto translations = readJson(file);
        for (const auto color : allCategoryColors())
            expectNonEmptyString(translations, categoryColorNameKey(color), file.filename().string());
    }
}
