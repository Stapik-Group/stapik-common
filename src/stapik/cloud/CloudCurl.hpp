#pragma once

#include <curl/curl.h>

namespace stapik::cloud
{
    // Protocol and TLS restrictions applied to every request sent to the cloud.
    void applySecurityOptions(CURL* curl);
}
