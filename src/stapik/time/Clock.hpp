#pragma once

#include <chrono>

namespace stapik::time
{
    using TimePoint = std::chrono::system_clock::time_point;

    class IClock
    {
    public:
        virtual ~IClock() = default;

        [[nodiscard]] virtual TimePoint now() const = 0;
    };

    class SystemClock final : public IClock
    {
    public:
        [[nodiscard]] TimePoint now() const override;
    };

    // The clock used wherever a caller does not pass one.
    [[nodiscard]] const IClock& systemClock();
}
