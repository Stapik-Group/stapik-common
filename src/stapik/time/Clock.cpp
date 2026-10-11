#include "Clock.hpp"

namespace stapik::time
{
    TimePoint SystemClock::now() const
    {
        return std::chrono::system_clock::now();
    }

    const IClock& systemClock()
    {
        static const SystemClock clock;
        return clock;
    }
}
