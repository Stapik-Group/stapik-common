#include "GitHubReleaseClient.hpp"

#include "stapik/http/CurlSecurity.hpp"

#include <curl/curl.h>

#include <format>
#include <memory>

namespace stapik::update
{
    namespace
    {
        constexpr long TIMEOUT_SECONDS = 15L;
        constexpr long CONNECT_TIMEOUT_SECONDS = 10L;
        constexpr std::size_t MAX_RESPONSE_BYTES = 2U * 1024U * 1024U;
        constexpr long HTTP_OK = 200;
        constexpr long HTTP_NOT_FOUND = 404;
        constexpr long HTTP_FORBIDDEN = 403;
        constexpr long HTTP_TOO_MANY_REQUESTS = 429;

        using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
        using HeaderList = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

        std::size_t writeCallback(const char* data, const std::size_t size, const std::size_t count, void* destination)
        {
            auto* body = static_cast<std::string*>(destination);
            const auto length = size * count;

            // Returning less than the received length makes libcurl abort the transfer.
            if (body->size() + length > MAX_RESPONSE_BYTES)
                return 0;

            body->append(data, length);
            return length;
        }
    }

    std::optional<ReleaseInfo> fetchLatestRelease(const std::string& repository, const std::string& userAgent)
    {
        const CurlHandle curl(curl_easy_init(), &curl_easy_cleanup);
        if (!curl)
            throw UpdateCheckException("Cannot initialize CURL!");

        // GitHub rejects requests without a User-Agent.
        HeaderList headers(nullptr, &curl_slist_free_all);
        for (const auto& header : { "User-Agent: " + userAgent, std::string("Accept: application/vnd.github+json"), std::string("X-GitHub-Api-Version: 2022-11-28") })
            headers.reset(curl_slist_append(headers.release(), header.c_str()));

        const auto url = latestReleaseApiUrl(repository);
        std::string response;

        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS);
        curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        http::applySecurityOptions(curl.get());

        if (const auto result = curl_easy_perform(curl.get()); result != CURLE_OK)
            throw UpdateCheckException(std::string("Release lookup failed: ") + curl_easy_strerror(result));

        long httpStatus = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpStatus);

        if (httpStatus == HTTP_NOT_FOUND)
            return std::nullopt;

        if (httpStatus == HTTP_FORBIDDEN || httpStatus == HTTP_TOO_MANY_REQUESTS)
            throw UpdateCheckException(std::format("Release lookup was rate limited (status {})", httpStatus));

        if (httpStatus != HTTP_OK)
            throw UpdateCheckException(std::format("Release lookup failed with status {}", httpStatus));

        return parseLatestRelease(response);
    }
}
