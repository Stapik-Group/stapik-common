#include "stapik/cloud/CloudStorageConfig.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
    std::string normalizedUrl(const std::string& apiUrl)
    {
        return CloudStorageConfig{ apiUrl, "key" }.normalizedApiUrl();
    }

    bool isSecure(const std::string& apiUrl)
    {
        return CloudStorageConfig{ apiUrl, "key" }.isSecure();
    }
}

TEST(CloudStorageConfigTest, NormalizedApiUrlStripsTrailingSlash)
{
    EXPECT_EQ(normalizedUrl("http://localhost:8080/"), "http://localhost:8080");
    EXPECT_EQ(normalizedUrl("http://localhost:8080"), "http://localhost:8080");
}

TEST(CloudStorageConfigTest, NormalizedApiUrlStripsRepeatedSlashesAndWhitespace)
{
    EXPECT_EQ(normalizedUrl("  https://example.com///  "), "https://example.com");
}

TEST(CloudStorageConfigTest, NormalizedApiUrlKeepsBasePath)
{
    EXPECT_EQ(normalizedUrl("https://example.com/base/"), "https://example.com/base");
}

TEST(CloudStorageConfigTest, NormalizedApiUrlOfBlankInputIsEmpty)
{
    EXPECT_TRUE(normalizedUrl("").empty());
    EXPECT_TRUE(normalizedUrl("   ").empty());
    EXPECT_TRUE(normalizedUrl(" / ").empty());
}

TEST(CloudStorageConfigTest, IsSecureRequiresHttpsScheme)
{
    EXPECT_TRUE(isSecure("https://example.com"));
    EXPECT_TRUE(isSecure("HTTPS://example.com"));
    EXPECT_TRUE(isSecure("  https://example.com/"));
    EXPECT_FALSE(isSecure("http://example.com"));
    EXPECT_FALSE(isSecure("https://"));
    EXPECT_FALSE(isSecure("https:///"));
    EXPECT_FALSE(isSecure(""));
}

TEST(CloudStorageConfigTest, IsConfiguredRequiresUrlAndKey)
{
    const CloudStorageConfig complete{ "https://example.com", "key" };
    const CloudStorageConfig missingUrl{ "", "key" };
    const CloudStorageConfig missingKey{ "https://example.com", "" };

    EXPECT_TRUE(complete.isConfigured());
    EXPECT_FALSE(missingUrl.isConfigured());
    EXPECT_FALSE(missingKey.isConfigured());
}
