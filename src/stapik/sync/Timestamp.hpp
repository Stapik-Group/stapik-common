#pragma once

#include <chrono>
#include <string>

namespace stapik::sync
{
    std::string toIso8601(std::chrono::system_clock::time_point tp);
    std::chrono::system_clock::time_point fromIso8601(const std::string& str);
}