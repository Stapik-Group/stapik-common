#include "stapik/update/Semver.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using stapik::update::parseSemver;
    using stapik::update::Semver;

    Semver parsed(const std::string& text)
    {
        const auto version = parseSemver(text);
        EXPECT_TRUE(version.has_value()) << text;
        return version.value_or(Semver{});
    }
}

TEST(SemverTest, ParsesTheCoreVersion)
{
    const auto [major, minor, patch, prerelease] = parsed("1.22.333");

    EXPECT_EQ(major, 1U);
    EXPECT_EQ(minor, 22U);
    EXPECT_EQ(patch, 333U);
    EXPECT_TRUE(prerelease.empty());
}

TEST(SemverTest, ParsesPrereleaseAndIgnoresBuildMetadata)
{
    const auto version = parsed("1.0.0-rc.1+build.5");

    EXPECT_EQ(version.prerelease, (std::vector<std::string>{ "rc", "1" }));
    EXPECT_EQ(version, parsed("1.0.0-rc.1"));
}

TEST(SemverTest, AcceptsLeadingVOfGitTags)
{
    EXPECT_EQ(parsed("v1.2.3"), parsed("1.2.3"));
    EXPECT_EQ(parsed("V1.2.3"), parsed("1.2.3"));
}

TEST(SemverTest, RejectsMalformedVersions)
{
    for (const auto* text : { "", "v", "1", "1.2", "1.2.3.4", "1.2.x", "01.2.3", "1.02.3", "1.2.3-", "1.2.3-a..b", "1.2.3-01",
                              "1.2.3-a_b", "1.2.3+", "1.2.3+a..b", "-1.2.3", "1.2.3_", "1.5.0_beta", "1.2.99999999999", " 1.2.3" })
    {
        EXPECT_FALSE(parseSemver(text).has_value()) << text;
    }
}

TEST(SemverTest, ComparesCoreNumbersNumerically)
{
    EXPECT_LT(parsed("1.2.3"), parsed("1.2.4"));
    EXPECT_LT(parsed("1.9.0"), parsed("1.10.0"));
    EXPECT_LT(parsed("1.99.99"), parsed("2.0.0"));
    EXPECT_GT(parsed("2.0.0"), parsed("1.99.99"));
}

TEST(SemverTest, ReleaseIsGreaterThanItsPrerelease)
{
    EXPECT_LT(parsed("1.0.0-rc.1"), parsed("1.0.0"));
    EXPECT_GT(parsed("1.0.0"), parsed("1.0.0-rc.1"));
    EXPECT_LT(parsed("1.0.0-rc.1"), parsed("1.0.1"));
}

TEST(SemverTest, FollowsThePrecedenceExampleFromTheSpecification)
{
    const std::vector<std::string> ascending = {
        "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0"
    };

    for (std::size_t index = 1; index < ascending.size(); ++index)
        EXPECT_LT(parsed(ascending[index - 1]), parsed(ascending[index])) << ascending[index - 1] << " < " << ascending[index];
}

TEST(SemverTest, NumericIdentifiersAreComparedWithoutOverflow)
{
    EXPECT_LT(parsed("1.0.0-99999999999999999999"), parsed("1.0.0-100000000000000000000"));
}

TEST(SemverTest, BuildMetadataDoesNotAffectEquality)
{
    EXPECT_EQ(parsed("1.2.3+a"), parsed("1.2.3+b"));
}

TEST(SemverTest, FormatsWithoutPrefixAndMetadata)
{
    EXPECT_EQ(parsed("v1.2.3").toString(), "1.2.3");
    EXPECT_EQ(parsed("1.2.3-rc.1+build").toString(), "1.2.3-rc.1");
}
