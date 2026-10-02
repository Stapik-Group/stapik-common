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

    ApiUrlStatus urlStatus(const std::string& apiUrl)
    {
        return CloudStorageConfig{ apiUrl, "key" }.urlStatus();
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

TEST(CloudStorageConfigTest, UrlStatusClassifiesAddresses)
{
    EXPECT_EQ(urlStatus("https://example.com"), ApiUrlStatus::Secure);
    EXPECT_EQ(urlStatus("HTTPS://example.com/"), ApiUrlStatus::Secure);
    EXPECT_EQ(urlStatus("http://localhost:8080/"), ApiUrlStatus::Insecure);
    EXPECT_EQ(urlStatus("  http://example.com  "), ApiUrlStatus::Insecure);
}

TEST(CloudStorageConfigTest, UrlStatusOfBlankInputIsEmpty)
{
    EXPECT_EQ(urlStatus(""), ApiUrlStatus::Empty);
    EXPECT_EQ(urlStatus("   "), ApiUrlStatus::Empty);
    EXPECT_EQ(urlStatus(" / "), ApiUrlStatus::Empty);
}

TEST(CloudStorageConfigTest, UrlStatusRejectsAddressesWithoutHttpScheme)
{
    EXPECT_EQ(urlStatus("example.com"), ApiUrlStatus::Invalid);
    EXPECT_EQ(urlStatus("ftp://example.com"), ApiUrlStatus::Invalid);
    EXPECT_EQ(urlStatus("localhost:8080"), ApiUrlStatus::Invalid);
    EXPECT_EQ(urlStatus("https://"), ApiUrlStatus::Invalid);
    EXPECT_EQ(urlStatus("http://"), ApiUrlStatus::Invalid);
    EXPECT_EQ(urlStatus("https:/example.com"), ApiUrlStatus::Invalid);
}
