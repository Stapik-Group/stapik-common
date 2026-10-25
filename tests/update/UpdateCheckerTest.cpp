#include "stapik/settings/SettingsStore.hpp"
#include "stapik/sync/Timestamp.hpp"
#include "stapik/update/UpdateChecker.hpp"

#include "support/FakeClock.hpp"
#include "support/MainLoopPump.hpp"
#include "support/TemporaryDirectory.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    using namespace stapik::update;
    using stapik::settings::SettingsStore;
    using stapik::test::pumpFor;
    using stapik::test::pumpUntil;

    constexpr auto LAST_CHECK = "update.lastCheck";
    constexpr auto ANNOUNCED_VERSION = "update.announcedVersion";

    ReleaseInfo releaseOf(const std::string& version)
    {
        return ReleaseInfo{
            .tag = "v" + version,
            .name = {},
            .pageUrl = "https://github.com/Stapik-Group/budgeting/releases/tag/v" + version,
            .version = *parseSemver(version)
        };
    }

    class UpdateCheckerTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            m_directory = stapik::test::createTemporaryDirectory("stapik-update");
            m_settingsPath = m_directory / "settings.json";
            m_settings = std::make_unique<SettingsStore>(m_settingsPath);
        }

        void TearDown() override
        {
            m_checker.reset();

            std::error_code errorCode;
            fs::remove_all(m_directory, errorCode);
        }

        void createChecker(const std::string& currentVersion)
        {
            m_checker = std::make_unique<UpdateChecker>(
                *parseSemver(currentVersion),
                [this]
                {
                    // Copied before the counter moves: the test changes them only after it saw the fetch.
                    const auto latest = m_latest;
                    const auto fails = m_fails;
                    ++m_fetches;

                    if (fails)
                        throw UpdateCheckException("offline");

                    return latest;
                },
                *m_settings,
                m_clock);

            m_checker->signalUpdateAvailable().connect([this](const ReleaseInfo& release) { m_announced.push_back(release.version.toString()); });
        }

        void checkAndWait() const
        {
            const int before = m_fetches;
            m_checker->check();
            ASSERT_TRUE(pumpUntil([this, before] { return m_fetches > before; }));
            pumpFor(50ms);
        }

        void checkAndExpectNoFetch() const
        {
            const int before = m_fetches;
            m_checker->check();
            pumpFor(100ms);
            EXPECT_EQ(m_fetches.load(), before);
        }

        void forgetLastCheck() const
        {
            m_settings->remove(LAST_CHECK);
        }

        fs::path m_directory;
        fs::path m_settingsPath;
        stapik::test::FakeClock m_clock;
        std::unique_ptr<SettingsStore> m_settings;
        std::unique_ptr<UpdateChecker> m_checker;
        std::optional<ReleaseInfo> m_latest;
        bool m_fails = false;
        std::atomic<int> m_fetches{ 0 };
        std::vector<std::string> m_announced;
    };
}

TEST_F(UpdateCheckerTest, NewerReleaseIsAnnounced)
{
    createChecker("1.0.0");
    m_latest = releaseOf("1.1.0");

    checkAndWait();

    EXPECT_EQ(m_announced, std::vector<std::string>{ "1.1.0" });
    EXPECT_TRUE(m_settings->contains(LAST_CHECK));
}

TEST_F(UpdateCheckerTest, CurrentOrOlderReleaseIsNotAnnounced)
{
    createChecker("1.0.0");

    for (const auto* version : { "1.0.0", "0.9.9", "1.0.0-rc.1" })
    {
        m_latest = releaseOf(version);
        forgetLastCheck();
        checkAndWait();
    }

    EXPECT_TRUE(m_announced.empty());
}

TEST_F(UpdateCheckerTest, EachVersionIsAnnouncedOnlyOnce)
{
    createChecker("1.0.0");
    m_latest = releaseOf("1.1.0");
    checkAndWait();

    forgetLastCheck();
    checkAndWait();
    EXPECT_EQ(m_announced.size(), 1U);

    m_latest = releaseOf("1.2.0");
    forgetLastCheck();
    checkAndWait();
    EXPECT_EQ(m_announced, (std::vector<std::string>{ "1.1.0", "1.2.0" }));
}

TEST_F(UpdateCheckerTest, AnnouncementIsPersistedAcrossRuns)
{
    createChecker("1.0.0");
    m_latest = releaseOf("1.1.0");
    checkAndWait();

    SettingsStore reloaded(m_settingsPath);
    ASSERT_EQ(reloaded.load(), stapik::settings::LoadStatus::Loaded);
    EXPECT_EQ(reloaded.get<std::string>(ANNOUNCED_VERSION, {}), "1.1.0");
}

TEST_F(UpdateCheckerTest, ChecksAtMostOncePerInterval)
{
    createChecker("1.0.0");
    m_latest = releaseOf("1.0.0");
    checkAndWait();
    EXPECT_EQ(m_fetches.load(), 1);

    m_clock.advance(UpdateChecker::CHECK_INTERVAL - 1min);
    checkAndExpectNoFetch();

    m_clock.advance(1min);
    checkAndWait();
    EXPECT_EQ(m_fetches.load(), 2);
}

TEST_F(UpdateCheckerTest, LastCheckInTheFutureDoesNotBlockTheCheck)
{
    createChecker("1.0.0");
    m_settings->set<std::string>(LAST_CHECK, stapik::sync::toIso8601(m_clock.now() + 48h));

    checkAndWait();

    EXPECT_EQ(m_fetches.load(), 1);
}

TEST_F(UpdateCheckerTest, FailedLookupIsRetriedAndNeverAnnounced)
{
    createChecker("1.0.0");
    m_latest = releaseOf("2.0.0");
    m_fails = true;
    checkAndWait();

    EXPECT_TRUE(m_announced.empty());
    EXPECT_FALSE(m_settings->contains(LAST_CHECK));

    m_fails = false;
    checkAndWait();
    EXPECT_EQ(m_announced, std::vector<std::string>{ "2.0.0" });
}

TEST_F(UpdateCheckerTest, RepositoryWithoutReleasesIsRecordedAsChecked)
{
    createChecker("1.0.0");

    checkAndWait();

    EXPECT_TRUE(m_announced.empty());
    EXPECT_TRUE(m_settings->contains(LAST_CHECK));
}

TEST_F(UpdateCheckerTest, CanBeTurnedOffInSettings)
{
    createChecker("1.0.0");
    m_latest = releaseOf("2.0.0");
    m_settings->set(CHECK_ENABLED_SETTING, false);

    checkAndExpectNoFetch();

    EXPECT_TRUE(m_announced.empty());
}

TEST_F(UpdateCheckerTest, ForAppNeedsAGithubRepositoryAndASemanticVersion)
{
    stapik::app::AppInfo appInfo;
    appInfo.internalName = "budgeting";
    appInfo.version = "1.4.0";
    appInfo.repositoryUrl = "https://github.com/Stapik-Group/budgeting";
    EXPECT_NE(UpdateChecker::forApp(appInfo, *m_settings), nullptr);

    appInfo.repositoryUrl = "https://example.com/budgeting";
    EXPECT_EQ(UpdateChecker::forApp(appInfo, *m_settings), nullptr);

    appInfo.repositoryUrl = "https://github.com/Stapik-Group/budgeting";
    appInfo.version = "1.4.0_";
    EXPECT_EQ(UpdateChecker::forApp(appInfo, *m_settings), nullptr);
}
