#pragma once

#include "stapik/time/Clock.hpp"

#include <chrono>
#include <optional>
#include <string_view>

namespace stapik::http
{
    [[nodiscard]] std::optional<std::chrono::seconds> parseRetryAfter(std::string_view headerValue, time::TimePoint now);
}
