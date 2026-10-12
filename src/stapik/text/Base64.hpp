#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace stapik::text
{
    [[nodiscard]] std::string base64Encode(std::string_view bytes);
    [[nodiscard]] std::optional<std::string> base64Decode(std::string_view text);
}
