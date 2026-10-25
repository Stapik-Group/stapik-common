#include "Cancellation.hpp"

#include <utility>

namespace stapik::task
{
    namespace
    {
        thread_local std::stop_token t_currentToken;
    }

    CancellationScope::CancellationScope(std::stop_token token) :
        m_previous(std::exchange(t_currentToken, std::move(token)))
    {}

    CancellationScope::~CancellationScope()
    {
        t_currentToken = std::move(m_previous);
    }

    bool isCancellationRequested()
    {
        return t_currentToken.stop_requested();
    }

    std::function<bool()> cancellationCheck(std::function<bool()> additional)
    {
        return [additional = std::move(additional)]
        {
            return isCancellationRequested() || (additional && additional());
        };
    }
}
