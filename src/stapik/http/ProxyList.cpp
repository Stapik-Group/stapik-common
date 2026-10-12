#include "ProxyList.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace stapik::http
{
    namespace
    {
        constexpr std::string_view SEPARATORS = "; \t";
        constexpr std::string_view LOCAL_NAMES_WILDCARD = "<local>";

        std::vector<std::string_view> entriesOf(std::string_view list)
        {
            std::vector<std::string_view> entries;
            while (!list.empty())
            {
                const auto start = list.find_first_not_of(SEPARATORS);
                if (start == std::string_view::npos)
                    break;

                list.remove_prefix(start);
                const auto end = std::min(list.find_first_of(SEPARATORS), list.size());
                entries.push_back(list.substr(0, end));
                list.remove_prefix(end);
            }
            return entries;
        }

        bool equalsIgnoreCase(const std::string_view left, const std::string_view right)
        {
            return std::ranges::equal(left, right, [](const char a, const char b)
            {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
        }

        std::string proxyUrlOf(const std::string_view address)
        {
            if (address.find("://") != std::string_view::npos)
                return std::string(address);

            return "http://" + std::string(address);
        }
    }

    std::string_view schemeOf(const std::string_view url)
    {
        const auto end = url.find("://");
        return end == std::string_view::npos ? std::string_view{} : url.substr(0, end);
    }

    std::string_view originOf(const std::string_view url)
    {
        const auto authorityStart = url.find("://");
        if (authorityStart == std::string_view::npos)
            return url;

        return url.substr(0, url.find('/', authorityStart + 3));
    }

    std::optional<std::string> proxyForScheme(const std::string_view proxyList, const std::string_view scheme)
    {
        std::optional<std::string> forAllSchemes;
        for (const auto entry : entriesOf(proxyList))
        {
            const auto equals = entry.find('=');
            if (equals == std::string_view::npos)
            {
                if (!forAllSchemes)
                    forAllSchemes = proxyUrlOf(entry);
                continue;
            }

            if (equalsIgnoreCase(entry.substr(0, equals), scheme))
                return proxyUrlOf(entry.substr(equals + 1));
        }
        return forAllSchemes;
    }

    std::string noProxyListOf(const std::string_view bypassList)
    {
        std::string result;
        for (auto entry : entriesOf(bypassList))
        {
            if (equalsIgnoreCase(entry, LOCAL_NAMES_WILDCARD))
                continue;

            while (entry.starts_with('*'))
                entry.remove_prefix(1);

            if (entry.empty())
                continue;

            if (!result.empty())
                result += ',';
            result += entry;
        }
        return result;
    }
}
