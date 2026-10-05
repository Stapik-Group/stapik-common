#include "stapik/app/SingleInstanceGuard.hpp"

#include <gtest/gtest.h>

// Only Windows can refuse a second instance; elsewhere GApplication keeps the application unique and the
// guard must never get in the way.
TEST(SingleInstanceGuardTest, AlwaysSucceedsOutsideWindows)
{
#ifdef _WIN32
    GTEST_SKIP() << "Windows behaviour is covered by the CI smoke test of the applications";
#else
    const auto first = stapik::app::SingleInstanceGuard::acquire("pl.stapik.test", "Test");
    const auto second = stapik::app::SingleInstanceGuard::acquire("pl.stapik.test", "Test");

    EXPECT_NE(first, nullptr);
    EXPECT_NE(second, nullptr);
#endif
}
