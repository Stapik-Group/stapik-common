#include "UserAgent.hpp"

namespace stapik::app
{
    namespace
    {
        constexpr auto FALLBACK_USER_AGENT = "stapik-common";
    }

    std::string userAgentOf(const AppInfo& appInfo)
    {
        if (appInfo.internalName.empty())
            return FALLBACK_USER_AGENT;

        return appInfo.version.empty() ? appInfo.internalName : appInfo.internalName + '/' + appInfo.version;
    }
}
