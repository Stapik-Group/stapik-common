#pragma once

#include "stapik/time/Clock.hpp"

#include <chrono>

namespace stapik::test
{
    // Stands still until the test moves it.
    class FakeClock final : public time::IClock
    {
    public:
        static constexpr auto DEFAULT_START = std::chrono::sys_days{std::chrono::year{2026} / std::chrono::January / 15}
            + std::chrono::hours{10} + std::chrono::minutes{30};

        explicit FakeClock(const time::TimePoint start = DEFAULT_START) :
            m_now(start)
        {}

        [[nodiscard]] time::TimePoint now() const override
        {
            return m_now;
        }

        void set(const time::TimePoint newTime)
        {
            m_now = newTime;
        }

        void advance(const std::chrono::system_clock::duration duration)
        {
            m_now += duration;
        }

    private:
        time::TimePoint m_now;
    };
}
