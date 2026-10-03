#include "stapik/document/SchemaMigrator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace
{
    using stapik::document::MigrationStatus;
    using stapik::document::SchemaMigrator;

    SchemaMigrator::Step appendToTrace(const std::string& marker)
    {
        return [marker](nlohmann::json& document)
        {
            document["trace"] = document.at("trace").get<std::string>() + marker;
        };
    }

    nlohmann::json startingDocument()
    {
        nlohmann::json document = nlohmann::json::object();
        document["trace"] = "";
        return document;
    }
}

TEST(SchemaMigratorTest, DocumentAtCurrentVersionIsUpToDate)
{
    const SchemaMigrator migrator(3);
    auto document = startingDocument();

    EXPECT_EQ(migrator.migrate(document, 3), MigrationStatus::UpToDate);
    EXPECT_EQ(document.at("trace").get<std::string>(), "");
}

TEST(SchemaMigratorTest, AppliesStepsInOrder)
{
    SchemaMigrator migrator(3);
    migrator.addStep(0, appendToTrace("1")).addStep(1, appendToTrace("2")).addStep(2, appendToTrace("3"));
    auto document = startingDocument();

    EXPECT_EQ(migrator.migrate(document, 0), MigrationStatus::Migrated);
    EXPECT_EQ(document.at("trace").get<std::string>(), "123");
}

TEST(SchemaMigratorTest, StartsFromTheGivenVersion)
{
    SchemaMigrator migrator(3);
    migrator.addStep(0, appendToTrace("1")).addStep(1, appendToTrace("2")).addStep(2, appendToTrace("3"));
    auto document = startingDocument();

    EXPECT_EQ(migrator.migrate(document, 2), MigrationStatus::Migrated);
    EXPECT_EQ(document.at("trace").get<std::string>(), "3");
}

TEST(SchemaMigratorTest, NewerVersionIsNotMigratedAndDocumentIsUntouched)
{
    const SchemaMigrator migrator(2);
    auto document = startingDocument();

    EXPECT_EQ(migrator.migrate(document, 5), MigrationStatus::FromNewerVersion);
    EXPECT_EQ(document.at("trace").get<std::string>(), "");
}

TEST(SchemaMigratorTest, MissingStepLeavesDocumentUntouched)
{
    SchemaMigrator migrator(3);
    migrator.addStep(0, appendToTrace("1"));
    auto document = startingDocument();

    EXPECT_EQ(migrator.migrate(document, 0), MigrationStatus::MissingStep);
    EXPECT_EQ(document.at("trace").get<std::string>(), "");
}

TEST(SchemaMigratorTest, FailingStepLeavesDocumentUntouched)
{
    SchemaMigrator migrator(3);
    migrator.addStep(0, appendToTrace("1"));
    migrator.addStep(1, [](nlohmann::json&) { throw std::runtime_error("boom"); });
    migrator.addStep(2, appendToTrace("3"));
    auto document = startingDocument();

    EXPECT_EQ(migrator.migrate(document, 0), MigrationStatus::StepFailed);
    EXPECT_EQ(document.at("trace").get<std::string>(), "");
}

TEST(SchemaMigratorTest, AddStepOutsideVersionRangeThrows)
{
    SchemaMigrator migrator(2);

    EXPECT_THROW(migrator.addStep(-1, appendToTrace("x")), std::invalid_argument);
    EXPECT_THROW(migrator.addStep(2, appendToTrace("x")), std::invalid_argument);
}

TEST(SchemaMigratorTest, HasStepReportsRegisteredSteps)
{
    SchemaMigrator migrator(3);
    migrator.addStep(1, appendToTrace("2"));

    EXPECT_TRUE(migrator.hasStep(1));
    EXPECT_FALSE(migrator.hasStep(0));
    EXPECT_EQ(migrator.currentVersion(), 3);
}
