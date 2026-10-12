#pragma once

#include "CloudStorageConfig.hpp"
#include "ICloudStorage.hpp"

#include <memory>
#include <string>

namespace stapik::cloud
{
    [[nodiscard]] std::unique_ptr<ICloudStorage> createCloudStorage(const CloudStorageConfig& config, const std::string& slotKey);
}
