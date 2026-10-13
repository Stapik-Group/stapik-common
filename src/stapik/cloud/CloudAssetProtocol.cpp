#include "CloudAssetProtocol.hpp"
#include "CloudPartitionProtocol.hpp"

#include "stapik/sync/Timestamp.hpp"

#include <nlohmann/json.hpp>

#include <glib.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <string_view>
#include <vector>

namespace stapik::cloud
{
    namespace
    {
        constexpr std::size_t MAX_FILENAME_BYTES = 255;
        constexpr std::string_view FORBIDDEN_FILENAME_CHARACTERS = "/\\:*?\"<>|";

        [[noreturn]] void rejectFilename(const std::string& reason)
        {
            throw InvalidAssetFilenameException("Invalid filename: " + reason);
        }

        // The server is written in Java: its notion of white space excludes the non-breaking spaces.
        bool isWhitespace(const gunichar character)
        {
            switch (g_unichar_type(character))
            {
                case G_UNICODE_SPACE_SEPARATOR:
                    return character != 0x00A0 && character != 0x2007 && character != 0x202F;
                case G_UNICODE_LINE_SEPARATOR:
                case G_UNICODE_PARAGRAPH_SEPARATOR:
                    return true;
                default:
                    return (character >= 0x09 && character <= 0x0D) || (character >= 0x1C && character <= 0x1F);
            }
        }

        bool isForbiddenCharacter(const gunichar character)
        {
            if (character < 0x80 && FORBIDDEN_FILENAME_CHARACTERS.find(static_cast<char>(character)) != std::string_view::npos)
                return true;

            switch (g_unichar_type(character))
            {
                case G_UNICODE_CONTROL:
                case G_UNICODE_FORMAT:
                case G_UNICODE_SURROGATE:
                case G_UNICODE_PRIVATE_USE:
                case G_UNICODE_UNASSIGNED:
                case G_UNICODE_LINE_SEPARATOR:
                case G_UNICODE_PARAGRAPH_SEPARATOR:
                    return true;
                default:
                    return false;
            }
        }

        std::vector<gunichar> codePointsOf(const std::string& text)
        {
            std::vector<gunichar> codePoints;
            for (const gchar* position = text.c_str(); *position != '\0'; position = g_utf8_next_char(position))
                codePoints.push_back(g_utf8_get_char(position));

            return codePoints;
        }

        bool isReservedDeviceName(const std::vector<gunichar>& codePoints)
        {
            std::vector<gunichar> base;
            for (const auto character : codePoints)
            {
                if (character == U'.')
                    break;

                base.push_back(character);
            }

            while (!base.empty() && isWhitespace(base.back()))
                base.pop_back();

            if (base.size() != 3 && base.size() != 4)
                return false;

            std::string name;
            for (const auto character : base)
            {
                if (character >= 0x80)
                    return false;

                name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
            }

            if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL")
                return true;

            return name.size() == 4 && (name.starts_with("COM") || name.starts_with("LPT")) && name[3] >= '1' && name[3] <= '9';
        }

        AssetInfo toAssetInfo(const nlohmann::json& item)
        {
            const auto updatedAtText = item.at("updatedAt").get<std::string>();
            const auto updatedAt = sync::parseIso8601(updatedAtText);
            if (!updatedAt)
                throw CloudStorageException("Invalid ISO-8601 timestamp: " + updatedAtText);

            return AssetInfo{
                .filename = item.at("filename").get<std::string>(),
                .mimeType = item.at("mimeType").get<std::string>(),
                .sizeBytes = item.at("sizeBytes").get<std::int64_t>(),
                .checksumSha256 = item.at("checksumSha256").get<std::string>(),
                .updatedAt = *updatedAt };
        }
    }

    std::string assetsUrl(const std::string& apiUrl, const std::string& slotKey)
    {
        return apiUrl + "/api/v1/assets/" + encodePathSegment(slotKey);
    }

    std::string assetUrl(const std::string& apiUrl, const std::string& slotKey, const std::string& filename)
    {
        return assetsUrl(apiUrl, slotKey) + "/" + encodePathSegment(filename);
    }

    std::string normalizeAssetFilename(const std::string& filename)
    {
        if (!g_utf8_validate(filename.data(), static_cast<gssize>(filename.size()), nullptr))
            rejectFilename("is not valid UTF-8");

        gchar* composed = g_utf8_normalize(filename.c_str(), -1, G_NORMALIZE_NFC);
        std::string normalized(composed);
        g_free(composed);

        const auto codePoints = codePointsOf(normalized);

        if (std::ranges::all_of(codePoints, isWhitespace))
            rejectFilename("must not be blank");

        if (normalized.size() > MAX_FILENAME_BYTES)
            rejectFilename(std::format("must not exceed {} bytes in UTF-8", MAX_FILENAME_BYTES));

        if (const auto forbidden = std::ranges::find_if(codePoints, isForbiddenCharacter); forbidden != codePoints.end())
            rejectFilename(std::format("contains forbidden character U+{:04X}", *forbidden));

        if (isWhitespace(codePoints.front()) || normalized.ends_with(' ') || normalized.ends_with('.'))
            rejectFilename("must not start with a space or end with a space or dot");

        if (isReservedDeviceName(codePoints))
            rejectFilename("is a reserved system name");

        return normalized;
    }

    std::optional<std::string> parseProblemDetail(const std::string& body)
    {
        const auto json = nlohmann::json::parse(body, nullptr, false);
        if (!json.is_object())
            return std::nullopt;

        const auto detail = json.find("detail");
        if (detail == json.end() || !detail->is_string())
            return std::nullopt;

        return detail->get<std::string>();
    }

    std::vector<AssetInfo> parseAssetList(const std::string& body)
    {
        try
        {
            const auto json = nlohmann::json::parse(body);

            std::vector<AssetInfo> result;
            if (!json.contains("assets"))
                return result;

            for (const auto& item : json.at("assets"))
                result.push_back(toAssetInfo(item));

            return result;
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
        }
    }

    AssetInfo parseAssetMetadata(const std::string& body)
    {
        try
        {
            return toAssetInfo(nlohmann::json::parse(body));
        }
        catch (const nlohmann::json::exception& e)
        {
            throw CloudStorageException(std::string("Failed to parse cloud response: ") + e.what());
        }
    }

    std::string sha256Hex(const AssetBytes& bytes)
    {
        // GLib refuses a null pointer even for an empty input.
        static constexpr guchar EMPTY_INPUT = 0;
        const guchar* data = bytes.empty() ? &EMPTY_INPUT : bytes.data();

        char* checksum = g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, bytes.size());
        std::string result(checksum);
        g_free(checksum);
        return result;
    }
}
