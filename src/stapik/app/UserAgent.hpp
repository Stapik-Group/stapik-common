#pragma once

#include "AppInfo.hpp"

#include <string>

namespace stapik::app
{
    [[nodiscard]] std::string userAgentOf(const AppInfo& appInfo);
}
