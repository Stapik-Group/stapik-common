#pragma once

#include "IAssetStorage.hpp"

#include <optional>
#include <string>
#include <vector>

namespace stapik::cloud
{
    [[nodiscard]] std::string assetsUrl(const std::string& apiUrl, const std::string& slotKey);
    [[nodiscard]] std::string assetUrl(const std::string& apiUrl, const std::string& slotKey, const std::string& filename);

    [[nodiscard]] std::vector<AssetInfo> parseAssetList(const std::string& body);
    [[nodiscard]] AssetInfo parseAssetMetadata(const std::string& body);

    // Brings a file name to the form the server stores (Unicode NFC) and checks it against the server rules:
    // not blank, at most 255 bytes in UTF-8, none of / \ : * ? " < > | and no control, format, private-use or
    // unassigned characters, no leading white space, no trailing space or dot, not a reserved device name
    // (CON, PRN, AUX, NUL, COM1-9, LPT1-9) before the first dot. Throws InvalidAssetFilenameException.
    [[nodiscard]] std::string normalizeAssetFilename(const std::string& filename);

    // The "detail" of an RFC 7807 error body, empty when the body has none.
    [[nodiscard]] std::optional<std::string> parseProblemDetail(const std::string& body);

    [[nodiscard]] std::string sha256Hex(const AssetBytes& bytes);
}
