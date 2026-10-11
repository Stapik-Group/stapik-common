# Clock

Code that needs the current time takes it from a `stapik::time::IClock` (`stapik/time/Clock.hpp`) instead of reading
the system clock, so a test can decide what time it is. Every place that accepts a clock takes it as the last
parameter and defaults to `stapik::time::systemClock()`, so production code does not pass anything.

```cpp
class FixedClock final : public stapik::time::IClock
{
public:
    [[nodiscard]] stapik::time::TimePoint now() const override { return m_now; }
    stapik::time::TimePoint m_now;
};
```

The repository tests use `FakeClock` (`tests/support/FakeClock.hpp`), which stays put until `set()` or `advance()` moves it.
