#pragma once

#include <algorithm>
#include <chrono>

namespace stapik::task
{
    class Deadline
    {
    public:
        explicit Deadline(const std::chrono::milliseconds timeout) :
            m_end(std::chrono::steady_clock::now() + timeout)
        {}

        [[nodiscard]] std::chrono::milliseconds remaining() const
        {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - std::chrono::steady_clock::now());
            return std::max(left, std::chrono::milliseconds::zero());
        }

    private:
        std::chrono::steady_clock::time_point m_end;
    };
}
