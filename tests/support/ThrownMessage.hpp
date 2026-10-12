#pragma once

#include <functional>
#include <string>

namespace stapik::test
{
    template<typename ExceptionType>
    [[nodiscard]] std::string thrownMessage(const std::function<void()>& action)
    {
        try
        {
            action();
        }
        catch (const ExceptionType& e)
        {
            return e.what();
        }
        return {};
    }
}
