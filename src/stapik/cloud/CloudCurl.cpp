#include "CloudCurl.hpp"

#include "stapik/storage/AppPaths.hpp"
#include "stapik/storage/PathText.hpp"

#include <filesystem>
#include <system_error>

namespace stapik::cloud
{
    void applySecurityOptions(CURL* curl)
    {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

#ifdef _WIN32
        // The CA bundle path compiled into libcurl points into the build machine's MSYS2 tree, which does not exist
        // on the user's PC. Trust the Windows certificate store, plus the bundle shipped next to the executable.
#if LIBCURL_VERSION_NUM >= 0x074700
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
        std::error_code errorCode;
        if (const auto bundle = AppPaths::executableDir() / "etc" / "ssl" / "certs" / "ca-bundle.crt"; std::filesystem::is_regular_file(bundle, errorCode))
            curl_easy_setopt(curl, CURLOPT_CAINFO, stapik::storage::pathText(bundle).c_str());
#endif
    }
}
