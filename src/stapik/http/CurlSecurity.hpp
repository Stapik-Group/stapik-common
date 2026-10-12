#pragma once

#include <curl/curl.h>

namespace stapik::http
{
    void applySecurityOptions(CURL* curl);
}
