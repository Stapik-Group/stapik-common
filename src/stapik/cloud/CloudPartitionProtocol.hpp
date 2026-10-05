#pragma once

#include "ICloudStorage.hpp"

#include <string>
#include <vector>

namespace stapik::cloud
{
    [[nodiscard]] std::string encodePathSegment(const std::string& segment);
    [[nodiscard]] std::string partitionsUrl(const std::string& documentUrl);
    [[nodiscard]] std::string partitionUrl(const std::string& documentUrl, const std::string& partition);
    [[nodiscard]] std::vector<CloudPartitionInfo> parsePartitionList(const std::string& body);
}
