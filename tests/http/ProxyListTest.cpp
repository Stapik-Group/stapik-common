#include "stapik/http/ProxyList.hpp"

#include <gtest/gtest.h>

namespace
{
    using namespace stapik::http;
}

TEST(ProxyListTest, SingleProxyServesEveryScheme)
{
    EXPECT_EQ(proxyForScheme("proxy.corp:8080", "https"), "http://proxy.corp:8080");
    EXPECT_EQ(proxyForScheme("proxy.corp:8080", "http"), "http://proxy.corp:8080");
}

TEST(ProxyListTest, ProxyIsChosenPerScheme)
{
    const std::string_view list = "http=plain.corp:80;https=secure.corp:443";

    EXPECT_EQ(proxyForScheme(list, "http"), "http://plain.corp:80");
    EXPECT_EQ(proxyForScheme(list, "https"), "http://secure.corp:443");
}

TEST(ProxyListTest, SchemeNamesIgnoreCase)
{
    EXPECT_EQ(proxyForScheme("HTTPS=secure.corp:443", "https"), "http://secure.corp:443");
}

TEST(ProxyListTest, SchemeWithoutAProxyGoesDirect)
{
    EXPECT_FALSE(proxyForScheme("http=plain.corp:80", "https").has_value());
    EXPECT_FALSE(proxyForScheme("socks=socks.corp:1080", "https").has_value());
    EXPECT_FALSE(proxyForScheme("", "https").has_value());
}

TEST(ProxyListTest, EntriesMayBeSeparatedByWhitespace)
{
    EXPECT_EQ(proxyForScheme("http=plain.corp:80 https=secure.corp:443", "https"), "http://secure.corp:443");
}

TEST(ProxyListTest, ExplicitProxySchemeIsKept)
{
    EXPECT_EQ(proxyForScheme("https://proxy.corp:8443", "https"), "https://proxy.corp:8443");
}

TEST(ProxyListTest, BypassListBecomesCommaSeparatedNoProxyList)
{
    EXPECT_EQ(noProxyListOf("intranet.corp;*.internal.corp;10.0.0.1"), "intranet.corp,.internal.corp,10.0.0.1");
}

TEST(ProxyListTest, LocalNamesWildcardCannotBeExpressedAndIsDropped)
{
    EXPECT_EQ(noProxyListOf("<local>;intranet.corp"), "intranet.corp");
    EXPECT_TRUE(noProxyListOf("<local>").empty());
    EXPECT_TRUE(noProxyListOf("").empty());
}

TEST(ProxyListTest, SchemeAndOriginAreTakenFromTheUrl)
{
    EXPECT_EQ(schemeOf("https://cloud.example/api/v1"), "https");
    EXPECT_EQ(originOf("https://cloud.example/api/v1"), "https://cloud.example");
    EXPECT_EQ(originOf("http://cloud.example:8080"), "http://cloud.example:8080");
    EXPECT_TRUE(schemeOf("cloud.example").empty());
}
