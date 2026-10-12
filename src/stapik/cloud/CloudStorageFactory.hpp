#pragma once

#include "CloudStorageConfig.hpp"
#include "ICloudStorage.hpp"

#include "stapik/http/IHttpClient.hpp"

#include <memory>
#include <string>

namespace stapik::cloud
{
    [[nodiscard]] std::unique_ptr<ICloudStorage> createCloudStorage(
        const CloudStorageConfig& config,
        const std::string& slotKey,
        const http::IHttpClient& httpClient = http::defaultHttpClient());
}
