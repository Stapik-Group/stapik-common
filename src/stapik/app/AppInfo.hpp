#pragma once

#include <string>

namespace stapik::app
{
    struct AppInfo
    {
        std::string applicationId;
        std::string internalName;
        std::string displayName;
        std::string version;
        std::string author;
        std::string repositoryUrl;
    };
}
