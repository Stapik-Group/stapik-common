#pragma once

#include <glib.h>

#include <chrono>
#include <functional>
#include <thread>

namespace stapik::test
{
    inline void pumpFor(const std::chrono::milliseconds duration)
    {
        const auto deadline = std::chrono::steady_clock::now() + duration;

        while (std::chrono::steady_clock::now() < deadline)
        {
            while (g_main_context_iteration(nullptr, FALSE))
            {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    inline bool pumpUntil(const std::function<bool()>& condition, const std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (!condition())
        {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;

            while (g_main_context_iteration(nullptr, FALSE))
            {
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        return true;
    }
}
