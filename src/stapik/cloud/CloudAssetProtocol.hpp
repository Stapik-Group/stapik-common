#pragma once

#include "IAssetStorage.hpp"

#include <string>
#include <vector>

namespace stapik::cloud
{
    [[nodiscard]] std::string assetsUrl(const std::string& apiUrl, const std::string& slotKey);
    [[nodiscard]] std::string assetUrl(const std::string& apiUrl, const std::string& slotKey, const std::string& filename);

    [[nodiscard]] std::vector<AssetInfo> parseAssetList(const std::string& body);
    [[nodiscard]] AssetInfo parseAssetMetadata(const std::string& body);

    [[nodiscard]] std::string sha256Hex(const AssetBytes& bytes);
}
