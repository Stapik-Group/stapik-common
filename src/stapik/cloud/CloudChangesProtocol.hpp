#pragma once

#include "ICloudStorage.hpp"

#include <chrono>
#include <string>

namespace stapik::cloud
{
    [[nodiscard]] std::string changesUrl(const std::string& apiUrl, std::chrono::system_clock::time_point since);
    [[nodiscard]] CloudChangeList parseChangeList(const std::string& body);
}
