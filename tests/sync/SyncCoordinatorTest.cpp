#include "stapik/sync/SyncCoordinator.hpp"

#include "support/FakeCloudStorage.hpp"
#include "support/SyncTestDocument.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>

namespace
{
    using namespace std::chrono;
    using stapik::sync::SyncCoordinator;
    using stapik::sync::SyncEnvelope;
    using stapik::sync::SyncState;
    using stapik::test::at;
    using stapik::test::cloudContent;
    using stapik::test::cloudDocument;
    using stapik::test::Entry;
    using stapik::test::FakeCloudStorage;
    using stapik::test::local;
    using stapik::test::TimePoint;

    class SyncCoordinatorTest : public testing::Test
    {
    protected:
        SyncCoordinatorTest()
        {
            m_fake.nextServerTime = at(1000);
        }

        FakeCloudStorage m_fake;
        SyncCoordinator<Entry> m_coordinator{ m_fake };
    };
}

// resolveOnConnect

TEST_F(SyncCoordinatorTest, ConnectWithoutCloudDocumentUploadsLocal)
{
    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(1000) });
    ASSERT_TRUE(m_fake.stored.has_value());
    EXPECT_EQ(m_fake.receivedBaselines.front(), TimePoint{});
}

TEST_F(SyncCoordinatorTest, ConnectWithEmptyPlaceholderUploadsLocalAgainstPlaceholderVersion)
{
    m_fake.stored = CloudDocument{ .content = nlohmann::json::object(), .updatedAt = at(500) };

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(m_fake.receivedBaselines.front(), at(500));
    EXPECT_EQ(m_fake.saveCalls, 1);
}

TEST_F(SyncCoordinatorTest, ConnectWithNewerCloudDocumentTakesTheServerVersion)
{
    m_fake.stored = cloudDocument("theirs", 200, 600);

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 100));

    EXPECT_EQ(outcome.state, SyncState::ServerWon);
    EXPECT_TRUE(outcome.replacesLocal());
    EXPECT_EQ(outcome.document.text, "theirs");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(600) });
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(SyncCoordinatorTest, ConnectWithOlderCloudDocumentUploadsLocal)
{
    m_fake.stored = cloudDocument("theirs", 100, 600);

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 200));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_FALSE(outcome.replacesLocal());
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(m_fake.receivedBaselines.front(), at(600));
    EXPECT_EQ(m_fake.stored->updatedAt, at(1000));
}

TEST_F(SyncCoordinatorTest, ConnectWithEqualTimestampsUploadsNothing)
{
    m_fake.stored = cloudDocument("same", 100, 600);

    const auto outcome = m_coordinator.resolveOnConnect(local("same", 100));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(600) });
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(SyncCoordinatorTest, SubSecondDifferenceIsIgnored)
{
    m_fake.stored = cloudDocument("same", 100, 600);
    auto document = local("same", 100);
    document.updated += milliseconds{ 700 };

    const auto outcome = m_coordinator.resolveOnConnect(document);

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(SyncCoordinatorTest, ConnectWhileOfflineKeepsLocal)
{
    m_fake.unreachable = true;

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::OfflineKeptLocal);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_FALSE(outcome.message.empty());
}

TEST_F(SyncCoordinatorTest, ConnectWithUnreadableCloudDocumentOverwritesNothing)
{
    m_fake.stored = CloudDocument{ .content = nlohmann::json::object(), .updatedAt = at(600) };
    m_fake.stored->content["unexpected"] = 1;

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 10));

    EXPECT_EQ(outcome.state, SyncState::RemoteUnreadable);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(m_fake.saveCalls, 0);
}

TEST_F(SyncCoordinatorTest, ConnectLosingARaceToANewerWriterAcceptsTheServer)
{
    m_fake.stored = cloudDocument("old", 100, 600);
    m_fake.beforeSave = [this] { m_fake.stored = cloudDocument("newest", 500, 700); };

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 300));

    EXPECT_EQ(outcome.state, SyncState::LostRaceAcceptedServer);
    EXPECT_TRUE(outcome.replacesLocal());
    EXPECT_EQ(outcome.document.text, "newest");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(700) });
    EXPECT_EQ(m_fake.saveCalls, 1);
}

TEST_F(SyncCoordinatorTest, ConflictWithOlderServerContentRetriesOnce)
{
    m_fake.stored = cloudDocument("old", 100, 600);
    m_fake.beforeSave = [this]
    {
        m_fake.stored = cloudDocument("racer", 150, 700);
        m_fake.beforeSave = nullptr;
    };

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 300));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(m_fake.saveCalls, 2);
    EXPECT_EQ(m_fake.receivedBaselines[0], at(600));
    EXPECT_EQ(m_fake.receivedBaselines[1], at(700));
    EXPECT_EQ(m_fake.stored->updatedAt, at(1000));
}

TEST_F(SyncCoordinatorTest, SecondConflictGivesUpAndAcceptsTheServer)
{
    m_fake.stored = cloudDocument("old", 100, 600);
    int writes = 0;
    m_fake.beforeSave = [this, &writes]
    {
        ++writes;
        m_fake.stored = cloudDocument("racer" + std::to_string(writes), 150, 600 + writes * 100);
    };

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 300));

    EXPECT_EQ(outcome.state, SyncState::LostRaceAcceptedServer);
    EXPECT_EQ(outcome.document.text, "racer2");
    EXPECT_EQ(m_fake.saveCalls, 2);
}

TEST_F(SyncCoordinatorTest, NetworkFailureDuringUploadKeepsLocal)
{
    m_fake.stored = cloudDocument("old", 100, 600);
    m_fake.failSaves = true;

    const auto outcome = m_coordinator.resolveOnConnect(local("mine", 300));

    EXPECT_EQ(outcome.state, SyncState::OfflineKeptLocal);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(m_fake.loadCalls, 1);
    EXPECT_EQ(m_fake.saveCalls, 1);
}

// pushLocalChange

TEST_F(SyncCoordinatorTest, PushUploadsAgainstTheKnownCloudVersion)
{
    m_fake.stored = cloudDocument("old", 100, 600);

    const auto outcome = m_coordinator.pushLocalChange(local("edited", 300, 600));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.text, "edited");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(1000) });
    EXPECT_EQ(m_fake.receivedBaselines.front(), at(600));
    EXPECT_EQ(m_fake.loadCalls, 0);
}

TEST_F(SyncCoordinatorTest, PushOfNeverSyncedDocumentUsesEpochBaseline)
{
    const auto outcome = m_coordinator.pushLocalChange(local("first", 10));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(m_fake.receivedBaselines.front(), TimePoint{});
}

TEST_F(SyncCoordinatorTest, PushRejectedBecauseServerIsNewerAcceptsTheServer)
{
    m_fake.stored = cloudDocument("theirs", 500, 800);

    const auto outcome = m_coordinator.pushLocalChange(local("mine", 300, 600));

    EXPECT_EQ(outcome.state, SyncState::LostRaceAcceptedServer);
    EXPECT_EQ(outcome.document.text, "theirs");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(800) });
    EXPECT_EQ(m_fake.saveCalls, 1);
}

TEST_F(SyncCoordinatorTest, PushRejectedButLocalStillNewerRetriesAndSucceeds)
{
    m_fake.stored = cloudDocument("theirs", 100, 800);

    const auto outcome = m_coordinator.pushLocalChange(local("mine", 300, 600));

    EXPECT_EQ(outcome.state, SyncState::Synchronized);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(m_fake.saveCalls, 2);
    EXPECT_EQ(m_fake.receivedBaselines[1], at(800));
}

TEST_F(SyncCoordinatorTest, PushWhileOfflineKeepsLocalUnchanged)
{
    m_fake.unreachable = true;

    const auto outcome = m_coordinator.pushLocalChange(local("mine", 300, 600));

    EXPECT_EQ(outcome.state, SyncState::OfflineKeptLocal);
    EXPECT_EQ(outcome.document.text, "mine");
    EXPECT_EQ(outcome.document.cloudBaseline, std::optional<TimePoint>{ at(600) });
}

TEST_F(SyncCoordinatorTest, UploadedContentIsASyncEnvelopeOfTheDocument)
{
    ASSERT_EQ(m_coordinator.pushLocalChange(local("payload text", 300)).state, SyncState::Synchronized);

    const auto envelope = SyncEnvelope::fromJson(m_fake.stored->content);
    EXPECT_EQ(envelope.lastUpdate, at(300));
    EXPECT_EQ(envelope.payload.at("text").get<std::string>(), "payload text");
}
