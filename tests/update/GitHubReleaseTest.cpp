#include "stapik/update/GitHubRelease.hpp"
#include "stapik/update/GitHubReleaseClient.hpp"

#include "support/FakeHttpClient.hpp"
#include "support/ThrownMessage.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    using namespace stapik::update;

    const std::string RELEASE_BODY = R"({
        "tag_name": "v1.4.2",
        "name": "Spring release",
        "html_url": "https://github.com/Stapik-Group/budgeting/releases/tag/v1.4.2",
        "draft": false,
        "prerelease": false,
        "body": "Release notes"
    })";
}

TEST(GitHubReleaseTest, RepositoryIsExtractedFromTheRepositoryUrl)
{
    EXPECT_EQ(githubRepositoryOf("https://github.com/Stapik-Group/budgeting"), "Stapik-Group/budgeting");
    EXPECT_EQ(githubRepositoryOf("https://github.com/Stapik-Group/budgeting/"), "Stapik-Group/budgeting");
    EXPECT_EQ(githubRepositoryOf("https://github.com/Stapik-Group/budgeting.git"), "Stapik-Group/budgeting");
    EXPECT_EQ(githubRepositoryOf("https://github.com/Stapik-Group/budgeting/releases"), "Stapik-Group/budgeting");
    EXPECT_EQ(githubRepositoryOf("HTTPS://GitHub.com/Stapik-Group/budgeting"), "Stapik-Group/budgeting");
}

TEST(GitHubReleaseTest, OtherUrlsAreNotGithubRepositories)
{
    for (const auto* url : { "", "https://github.com/", "https://github.com/Stapik-Group", "https://github.com//budgeting",
                             "http://github.com/Stapik-Group/budgeting", "https://gitlab.com/Stapik-Group/budgeting",
                             "https://github.com.evil.example/Stapik-Group/budgeting", "https://github.com/Stapik-Group/../budgeting",
                             "https://github.com/Stapik Group/budgeting" })
    {
        EXPECT_FALSE(githubRepositoryOf(url).has_value()) << url;
    }
}

TEST(GitHubReleaseTest, LatestReleaseIsQueriedThroughTheApi)
{
    EXPECT_EQ(latestReleaseApiUrl("Stapik-Group/budgeting"), "https://api.github.com/repos/Stapik-Group/budgeting/releases/latest");
}

TEST(GitHubReleaseTest, ReleaseIsParsed)
{
    const auto [tag, name, pageUrl, version] = parseLatestRelease(RELEASE_BODY);

    EXPECT_EQ(tag, "v1.4.2");
    EXPECT_EQ(name, "Spring release");
    EXPECT_EQ(pageUrl, "https://github.com/Stapik-Group/budgeting/releases/tag/v1.4.2");
    EXPECT_EQ(version.toString(), "1.4.2");
}

TEST(GitHubReleaseTest, ReleaseNameIsOptional)
{
    const auto release = parseLatestRelease(R"({"tag_name":"1.0.0","name":null,"html_url":"https://github.com/o/r/releases/tag/1.0.0"})");

    EXPECT_TRUE(release.name.empty());
    EXPECT_EQ(release.version.toString(), "1.0.0");
}

TEST(GitHubReleaseTest, MalformedResponsesAreReportedAsUpdateCheckErrors)
{
    for (const auto* body : { "not json", "{}", R"({"tag_name":"v1.0.0"})", R"({"html_url":"https://github.com/o/r"})",
                              R"({"tag_name":1,"html_url":"https://github.com/o/r"})" })
    {
        EXPECT_THROW(static_cast<void>(parseLatestRelease(body)), UpdateCheckException) << body;
    }
}

TEST(GitHubReleaseTest, TagThatIsNotASemanticVersionIsRejected)
{
    EXPECT_THROW(static_cast<void>(parseLatestRelease(R"({"tag_name":"nightly","html_url":"https://github.com/o/r"})")), UpdateCheckException);
}

TEST(GitHubReleaseTest, PageThatIsNotHttpsIsRejected)
{
    for (const auto* url : { "http://example.com/release", "ftp://example.com/release", "example.com/release", "" })
    {
        const std::string body = std::string(R"({"tag_name":"v1.0.0","html_url":")") + url + R"("})";
        EXPECT_THROW(static_cast<void>(parseLatestRelease(body)), UpdateCheckException) << url;
    }
}

TEST(GitHubReleaseClientTest, LatestReleaseIsRequestedWithTheApplicationUserAgent)
{
    const stapik::test::FakeHttpClient http;
    http.respondWith(200, RELEASE_BODY);

    const auto release = fetchLatestRelease("Stapik-Group/budgeting", "budgeting/1.0.0", http);

    ASSERT_TRUE(release.has_value());
    EXPECT_EQ(release->tag, "v1.4.2");
    ASSERT_EQ(http.requests.size(), 1U);
    EXPECT_EQ(http.requests.front().url, "https://api.github.com/repos/Stapik-Group/budgeting/releases/latest");
    EXPECT_EQ(http.requests.front().headers.at("User-Agent"), "budgeting/1.0.0");
}

TEST(GitHubReleaseClientTest, RepositoryWithoutReleasesHasNoLatestRelease)
{
    const stapik::test::FakeHttpClient http;
    http.respondWith(404);

    EXPECT_FALSE(fetchLatestRelease("Stapik-Group/budgeting", "budgeting/1.0.0", http).has_value());
}

TEST(GitHubReleaseClientTest, RateLimitAndOtherStatusesAreReported)
{
    const stapik::test::FakeHttpClient http;
    http.respondWith(403);
    http.respondWith(500);

    EXPECT_EQ(stapik::test::thrownMessage<UpdateCheckException>([&] { static_cast<void>(fetchLatestRelease("o/r", "ua", http)); }), "Release lookup was rate limited (status 403)");
    EXPECT_EQ(stapik::test::thrownMessage<UpdateCheckException>([&] { static_cast<void>(fetchLatestRelease("o/r", "ua", http)); }), "Release lookup failed with status 500");
}

TEST(GitHubReleaseClientTest, TransportFailureIsReportedWithTheReason)
{
    const stapik::test::FakeHttpClient http;
    http.failWith("Could not resolve host");

    EXPECT_EQ(stapik::test::thrownMessage<UpdateCheckException>([&] { static_cast<void>(fetchLatestRelease("o/r", "ua", http)); }), "Release lookup failed: Could not resolve host");
}
