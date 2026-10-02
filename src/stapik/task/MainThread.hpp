#pragma once

#include <functional>

namespace stapik::task
{
    void postToMainThread(std::function<void()> callback);
}
