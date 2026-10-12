#pragma once

#include <curl/curl.h>

#include <string>

namespace stapik::http
{
    void applyProxyOptions(CURL* curl, const std::string& url);
}
