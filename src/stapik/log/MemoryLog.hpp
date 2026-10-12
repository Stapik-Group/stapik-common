#pragma once

#include "RingBufferSink.hpp"

#include <cstddef>
#include <memory>

namespace stapik::log
{
    inline constexpr std::size_t MEMORY_LOG_CAPACITY = 2000;
    std::shared_ptr<RingBufferSink> installMemoryLog(std::size_t capacity = MEMORY_LOG_CAPACITY);
    [[nodiscard]] std::shared_ptr<RingBufferSink> memoryLog();
}
