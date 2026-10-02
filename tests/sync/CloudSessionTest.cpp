#include "stapik/sync/CloudSession.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/MainLoopPump.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using namespace std::chrono_literals;
    using stapik::sync::AsyncSyncOptions;
    using stapik::sync::CloudSession;
    using stapik::sync::CloudSessionHooks;
    using stapik::sync::SyncEnvelope;
    using stapik::sync::SyncStatus;
    using stapik::test::at;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::local;
    using stapik::test::pumpFor;
    using stapik::test::pumpUntil;
    using stapik::test::TimePoint;

    class StorageProxy final : public ICloudStorage
    {
    public:
        explicit StorageProxy(FakeCloudStorage& target) :
            m_target(target)
        {}

        [[nodiscard]] std::optional<CloudDocument> loadDocument() const override
        {
            return m_target.loadDocument();
        }

        [[nodiscard]] CloudWriteResult saveDocument(const nlohmann::json& data, const TimePoint clientLastKnownUpdate) const override
        {
            return m_target.saveDocument(data, clientLastKnownUpdate);
        }

    private:
        FakeCloudStorage& m_target;
    };

    CloudStorageConfig validConfig(const std::string& url = "https://example.com")
    {
        return CloudStorageConfig{ .apiUrl = url, .apiKey = "key" };
    }

    class CloudSessionTest : public testing::Test
    {
    protected:
        CloudSessionTest()
        {
            m_fake.nextServerTime = at(1000);
            m_fake.serverTimeStep = 1s;
        }

        CloudSessionHooks hooks()
        {
            CloudSessionHooks result;
            result.loadConfig = [this] { return m_storedConfig; };
            result.saveConfig = [this](const CloudStorageConfig& config)
            {
                m_savedConfigs.push_back(config);
                return m_saveSucceeds;
            };
            result.createStorage = [this](const CloudStorageConfig& config) -> std::unique_ptr<ICloudStorage>
            {
                m_createdFor.push_back(config);
                return std::make_unique<StorageProxy>(m_fake);
            };
            return result;
        }

        void watch(CloudSession<Entry>& session)
        {
            session.signalDocumentReplaced().connect([this](const Entry& document) { m_replaced.push_back(document); });
            session.signalBaselineChanged().connect([this](const TimePoint baseline) { m_baselines.push_back(baseline); });
            session.signalStatusChanged().connect([this](const SyncStatus status) { m_statuses.push_back(status); });
        }

        [[nodiscard]] std::string storedText() const
        {
            return SyncEnvelope::fromJson(m_fake.stored->content).payload.at("text").get<std::string>();
        }

        AsyncSyncOptions m_options{ 30ms, 40ms, 160ms };
        FakeCloudStorage m_fake;
        std::optional<CloudStorageConfig> m_storedConfig;
        bool m_saveSucceeds = true;
        std::vector<CloudStorageConfig> m_savedConfigs;
        std::vector<CloudStorageConfig> m_createdFor;
        std::vector<Entry> m_replaced;
        std::vector<TimePoint> m_baselines;
        std::vector<SyncStatus> m_statuses;
    };
}

TEST_F(CloudSessionTest, WithoutStoredConfigurationItIsNotConnected)
{
    CloudSession<Entry> session(hooks(), m_options);

    EXPECT_FALSE(session.isConnected());
    EXPECT_FALSE(session.config().has_value());
    EXPECT_TRUE(m_createdFor.empty());
}

TEST_F(CloudSessionTest, NotConnectedSessionIgnoresRequests)
{
    CloudSession<Entry> session(hooks(), m_options);

    session.pushChange(local("x", 100));
    session.syncNow(local("x", 100));
    session.flush();
    pumpFor(100ms);

    EXPECT_EQ(m_fake.saveCalls, 0);
    EXPECT_EQ(m_fake.loadCalls, 0);
    EXPECT_FALSE(session.hasPendingWork());
    EXPECT_EQ(session.status(), SyncStatus::Idle);
}

TEST_F(CloudSessionTest, StoredConfigurationConnectsAtConstruction)
{
    m_storedConfig = validConfig();

    CloudSession<Entry> session(hooks(), m_options);

    EXPECT_TRUE(session.isConnected());
    ASSERT_TRUE(session.config().has_value());
    EXPECT_EQ(session.config()->apiUrl, "https://example.com");
    EXPECT_EQ(m_createdFor.size(), 1u);
}

TEST_F(CloudSessionTest, IncompleteStoredConfigurationDoesNotConnect)
{
    m_storedConfig = CloudStorageConfig{ .apiUrl = "https://example.com", .apiKey = "" };

    CloudSession<Entry> session(hooks(), m_options);

    EXPECT_FALSE(session.isConnected());
    EXPECT_TRUE(m_createdFor.empty());
}

TEST_F(CloudSessionTest, ConnectWithSavesTheConfigurationAndSynchronizes)
{
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);

    EXPECT_TRUE(session.connectWith(validConfig(), local("mine", 100)));

    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));
    ASSERT_EQ(m_savedConfigs.size(), 1u);
    EXPECT_EQ(m_savedConfigs.front().apiKey, "key");
    EXPECT_TRUE(session.isConnected());
    EXPECT_EQ(storedText(), "mine");
    EXPECT_EQ(m_baselines.front(), at(1000));
    EXPECT_TRUE(m_replaced.empty());
}

TEST_F(CloudSessionTest, ConnectWithAnIncompleteConfigurationIsRejected)
{
    CloudSession<Entry> session(hooks(), m_options);

    EXPECT_FALSE(session.connectWith(CloudStorageConfig{ .apiUrl = "", .apiKey = "key" }, local("mine", 100)));

    EXPECT_FALSE(session.isConnected());
    EXPECT_TRUE(m_savedConfigs.empty());
    EXPECT_TRUE(m_createdFor.empty());
}

TEST_F(CloudSessionTest, ConnectStillSucceedsWhenSavingTheConfigurationFails)
{
    m_saveSucceeds = false;
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);

    EXPECT_TRUE(session.connectWith(validConfig(), local("mine", 100)));

    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));
    EXPECT_TRUE(session.isConnected());
}

TEST_F(CloudSessionTest, ConnectFailsWhenNoStorageCanBeCreated)
{
    auto brokenHooks = hooks();
    brokenHooks.createStorage = [](const CloudStorageConfig&) -> std::unique_ptr<ICloudStorage> { return nullptr; };
    CloudSession<Entry> session(brokenHooks, m_options);

    EXPECT_FALSE(session.connectWith(validConfig(), local("mine", 100)));
    EXPECT_FALSE(session.isConnected());
}

TEST_F(CloudSessionTest, NewerServerDocumentReplacesTheLocalOne)
{
    m_fake.stored = cloudDocument("theirs", 500, 600);
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);

    session.connectWith(validConfig(), local("mine", 100));

    ASSERT_TRUE(pumpUntil([this] { return !m_replaced.empty(); }));
    EXPECT_EQ(m_replaced.front().text, "theirs");
    EXPECT_EQ(m_replaced.front().cloudBaseline, std::optional<TimePoint>{ at(600) });
    EXPECT_TRUE(m_baselines.empty());
}

TEST_F(CloudSessionTest, PushChangeUploadsAndReportsTheNewBaseline)
{
    m_storedConfig = validConfig();
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);

    session.pushChange(local("edited", 100));

    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));
    EXPECT_EQ(storedText(), "edited");
    EXPECT_EQ(m_baselines.front(), at(1000));
}

TEST_F(CloudSessionTest, StatusChangesAreForwarded)
{
    m_storedConfig = validConfig();
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);

    session.pushChange(local("edited", 100));
    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));

    EXPECT_TRUE(std::ranges::find(m_statuses, SyncStatus::Syncing) != m_statuses.end());
    EXPECT_EQ(session.status(), SyncStatus::Idle);
}

TEST_F(CloudSessionTest, OfflinePeriodIsReportedAndRecovers)
{
    m_storedConfig = validConfig();
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);
    m_fake.unreachable = true;

    session.pushChange(local("edited", 100));
    ASSERT_TRUE(pumpUntil([this] { return std::ranges::find(m_statuses, SyncStatus::Offline) != m_statuses.end(); }));
    EXPECT_TRUE(session.hasPendingWork());

    m_fake.unreachable = false;
    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));
    EXPECT_EQ(storedText(), "edited");
}

TEST_F(CloudSessionTest, ConnectingAgainReplacesTheStorageAndConfiguration)
{
    m_storedConfig = validConfig("https://first.example.com");
    CloudSession<Entry> session(hooks(), m_options);
    watch(session);

    EXPECT_TRUE(session.connectWith(validConfig("https://second.example.com"), local("mine", 100)));

    ASSERT_TRUE(pumpUntil([this] { return !m_baselines.empty(); }));
    EXPECT_EQ(m_createdFor.size(), 2u);
    EXPECT_EQ(session.config()->apiUrl, "https://second.example.com");
}

TEST_F(CloudSessionTest, DestroyingTheSessionWithPendingChangesIsSafe)
{
    m_storedConfig = validConfig();

    {
        CloudSession<Entry> session(hooks(), m_options);
        watch(session);
        session.pushChange(local("pending", 100));
    }

    pumpFor(150ms);

    EXPECT_EQ(m_fake.saveCalls, 0);
    EXPECT_TRUE(m_baselines.empty());
}
