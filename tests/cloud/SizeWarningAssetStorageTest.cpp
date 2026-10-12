#include "stapik/cloud/SizeWarningAssetStorage.hpp"

#include "support/FakeAssetStorage.hpp"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

namespace
{
    using namespace stapik::cloud;

    constexpr std::int64_t MAX_BYTES = 1000;

    AssetBytes bytesOfSize(const std::size_t size)
    {
        return AssetBytes(size, 0x2A);
    }

    class SizeWarningAssetStorageTest : public testing::Test
    {
    protected:
        stapik::test::FakeAssetStorage m_inner;
        std::vector<AssetSizeWarning> m_warnings;
        SizeWarningAssetStorage m_storage{ m_inner, AssetSizeLimit{ .maxBytes = MAX_BYTES }, [this](const AssetSizeWarning& warning) { m_warnings.push_back(warning); } };
    };
}

TEST(AssetSizeLimitTest, ClassifiesSizesAgainstTheLimit)
{
    constexpr AssetSizeLimit limit{ .maxBytes = MAX_BYTES };

    EXPECT_EQ(limit.assess(0), AssetSizeStatus::Within);
    EXPECT_EQ(limit.assess(899), AssetSizeStatus::Within);
    EXPECT_EQ(limit.assess(900), AssetSizeStatus::NearLimit);
    EXPECT_EQ(limit.assess(1000), AssetSizeStatus::NearLimit);
    EXPECT_EQ(limit.assess(1001), AssetSizeStatus::OverLimit);
}

TEST(AssetSizeLimitTest, WarnPercentMovesTheThreshold)
{
    constexpr AssetSizeLimit limit{ .maxBytes = MAX_BYTES, .warnPercent = 50 };

    EXPECT_EQ(limit.assess(499), AssetSizeStatus::Within);
    EXPECT_EQ(limit.assess(500), AssetSizeStatus::NearLimit);
}

TEST(AssetSizeLimitTest, UnknownLimitNeverWarns)
{
    constexpr AssetSizeLimit limit{};

    EXPECT_EQ(limit.assess(1'000'000'000), AssetSizeStatus::Within);
}

TEST_F(SizeWarningAssetStorageTest, SmallFileIsUploadedWithoutWarning)
{
    std::ignore = m_storage.uploadAsset("a.jpg", "image/jpeg", bytesOfSize(100));

    EXPECT_TRUE(m_warnings.empty());
    EXPECT_EQ(m_inner.uploaded, std::vector<std::string>{ "a.jpg" });
}

TEST_F(SizeWarningAssetStorageTest, FileNearTheLimitWarnsAndIsStillUploaded)
{
    std::ignore = m_storage.uploadAsset("a.jpg", "image/jpeg", bytesOfSize(950));

    ASSERT_EQ(m_warnings.size(), 1U);
    EXPECT_EQ(m_warnings.front().filename, "a.jpg");
    EXPECT_EQ(m_warnings.front().sizeBytes, 950);
    EXPECT_EQ(m_warnings.front().limit.maxBytes, MAX_BYTES);
    EXPECT_EQ(m_warnings.front().status, AssetSizeStatus::NearLimit);
    EXPECT_EQ(m_inner.uploaded, std::vector<std::string>{ "a.jpg" });
}

TEST_F(SizeWarningAssetStorageTest, FileOverTheLimitWarnsBeforeTheServerRefusesIt)
{
    m_inner.tooLarge.insert("big.jpg");

    EXPECT_THROW(std::ignore = m_storage.uploadAsset("big.jpg", "image/jpeg", bytesOfSize(2000)), AssetTooLargeException);

    ASSERT_EQ(m_warnings.size(), 1U);
    EXPECT_EQ(m_warnings.front().status, AssetSizeStatus::OverLimit);
}

TEST_F(SizeWarningAssetStorageTest, WorksWithoutAWarningHandler)
{
    const SizeWarningAssetStorage silent(m_inner, AssetSizeLimit{ .maxBytes = MAX_BYTES });

    EXPECT_NO_THROW(std::ignore = silent.uploadAsset("a.jpg", "image/jpeg", bytesOfSize(950)));
}

TEST_F(SizeWarningAssetStorageTest, OtherOperationsAreForwarded)
{
    m_inner.put("a.txt", "abc");

    EXPECT_EQ(m_storage.listAssets().size(), 1U);
    EXPECT_TRUE(m_storage.downloadAsset("a.txt").has_value());
    EXPECT_TRUE(m_storage.deleteAsset("a.txt"));
    EXPECT_TRUE(m_inner.files.empty());
}
