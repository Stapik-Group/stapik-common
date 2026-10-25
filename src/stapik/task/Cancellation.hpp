#pragma once

#include <functional>
#include <stop_token>

namespace stapik::task
{
    class CancellationScope
    {
    public:
        explicit CancellationScope(std::stop_token token);
        ~CancellationScope();

        CancellationScope(const CancellationScope&) = delete;
        CancellationScope& operator=(const CancellationScope&) = delete;

    private:
        std::stop_token m_previous;
    };

    [[nodiscard]] bool isCancellationRequested();

    [[nodiscard]] std::function<bool()> cancellationCheck(std::function<bool()> additional = {});
}
