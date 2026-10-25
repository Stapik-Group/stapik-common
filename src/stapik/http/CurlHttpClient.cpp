#include "CurlHttpClient.hpp"
#include "CurlProxy.hpp"
#include "CurlSecurity.hpp"
#include "RetryAfter.hpp"

#include "stapik/time/Clock.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace stapik::http
{
    namespace
    {
        void initializeCurl()
        {
            static const bool initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
            (void)initialized;
        }

        class ThreadCurlHandle
        {
        public:
            ThreadCurlHandle() = default;

            ~ThreadCurlHandle()
            {
                curl_easy_cleanup(m_handle);
            }

            ThreadCurlHandle(const ThreadCurlHandle &) = delete;

            ThreadCurlHandle &operator=(const ThreadCurlHandle &) = delete;

            [[nodiscard]] CURL *acquire()
            {
                if (m_busy)
                    return nullptr;

                if (m_handle == nullptr)
                {
                    initializeCurl();
                    m_handle = curl_easy_init();
                }

                m_busy = m_handle != nullptr;
                return m_handle;
            }

            void release()
            {
                m_busy = false;
            }

            [[nodiscard]] bool owns(const CURL *handle) const { return handle == m_handle; }

        private:
            CURL *m_handle = nullptr;
            bool m_busy = false;
        };

        ThreadCurlHandle &threadCurlHandle()
        {
            thread_local ThreadCurlHandle handle;
            return handle;
        }

        class CurlTransfer
        {
        public:
            CurlTransfer() : m_handle(threadCurlHandle().acquire())
            {
                if (m_handle == nullptr)
                {
                    initializeCurl();
                    m_handle = curl_easy_init();
                }
            }

            ~CurlTransfer()
            {
                if (m_handle != nullptr)
                    curl_easy_reset(m_handle);

                curl_mime_free(m_mime);
                curl_slist_free_all(m_headers);

                if (threadCurlHandle().owns(m_handle))
                    threadCurlHandle().release();
                else
                    curl_easy_cleanup(m_handle);
            }

            CurlTransfer(const CurlTransfer &) = delete;

            CurlTransfer &operator=(const CurlTransfer &) = delete;

            [[nodiscard]] CURL *handle() const { return m_handle; }

            void addHeader(const std::string &header)
            {
                m_headers = curl_slist_append(m_headers, header.c_str());
            }

            [[nodiscard]] curl_slist *headers() const { return m_headers; }

            [[nodiscard]] curl_mime *createMime()
            {
                m_mime = curl_mime_init(m_handle);
                return m_mime;
            }

        private:
            CURL *m_handle;
            curl_slist *m_headers = nullptr;
            curl_mime *m_mime = nullptr;
        };

        struct ResponseSink
        {
            Bytes body;
            std::size_t limit = std::numeric_limits<std::size_t>::max();
            bool limitExceeded = false;
        };

        struct ResponseHeaders
        {
            std::map<std::string, std::string> values;
        };

        std::string_view trimmed(const std::string_view text)
        {
            constexpr std::string_view WHITESPACE = " \t\r\n";

            const auto first = text.find_first_not_of(WHITESPACE);
            if (first == std::string_view::npos)
                return {};

            const auto last = text.find_last_not_of(WHITESPACE);
            return text.substr(first, last - first + 1);
        }

        std::string lowerCased(const std::string_view text)
        {
            std::string result(text);
            std::ranges::transform(result, result.begin(), [](const char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
            return result;
        }

        std::size_t collectHeader(const char *data, const std::size_t size, const std::size_t count, void *destination)
        {
            constexpr std::string_view STATUS_LINE_PREFIX = "HTTP/";

            auto &[values] = *static_cast<ResponseHeaders *>(destination);
            const std::string_view line(data, size * count);

            if (line.starts_with(STATUS_LINE_PREFIX))
            {
                values.clear();
                return line.size();
            }

            if (const auto colon = line.find(':'); colon != std::string_view::npos && colon > 0)
                values[lowerCased(trimmed(line.substr(0, colon)))] = std::string(trimmed(line.substr(colon + 1)));

            return line.size();
        }

        std::size_t collectBody(const char *data, const std::size_t size, const std::size_t count, void *destination)
        {
            auto &sink = *static_cast<ResponseSink *>(destination);
            const auto length = size * count;

            if (length > sink.limit - sink.body.size())
            {
                sink.limitExceeded = true;
                return 0;
            }

            const auto *bytes = reinterpret_cast<const std::uint8_t *>(data);
            sink.body.insert(sink.body.end(), bytes, bytes + length);
            return length;
        }

        long seconds(const std::chrono::seconds duration)
        {
            return duration.count();
        }

        void applyTimeouts(CURL *curl, const Request &request)
        {
            constexpr long MINIMAL_SPEED_BYTES_PER_SECOND = 1L;

            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, seconds(request.timeout));
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, seconds(request.connectTimeout));
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, MINIMAL_SPEED_BYTES_PER_SECOND);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, seconds(request.stallTimeout));
        }

        void applyUserAgent(CURL *curl, const CurlHttpClient::UserAgentProvider &userAgent)
        {
            if (!userAgent)
                return;

            if (const auto text = userAgent(); !text.empty())
                curl_easy_setopt(curl, CURLOPT_USERAGENT, text.c_str());
        }

        int reportProgress(void *userData, const curl_off_t downloadTotal, const curl_off_t downloaded, const curl_off_t uploadTotal, const curl_off_t uploaded)
        {
            const auto &request = *static_cast<const Request *>(userData);

            if (request.onProgress)
            {
                request.onProgress(TransferProgress{
                    .uploadedBytes = static_cast<std::uint64_t>(uploaded),
                    .uploadTotalBytes = static_cast<std::uint64_t>(uploadTotal),
                    .downloadedBytes = static_cast<std::uint64_t>(downloaded),
                    .downloadTotalBytes = static_cast<std::uint64_t>(downloadTotal) });
            }

            return request.isCancelled && request.isCancelled() ? 1 : 0;
        }

        void applyProgress(CURL *curl, const Request &request)
        {
            if (!request.onProgress && !request.isCancelled)
                return;

            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, reportProgress);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &request);
        }

        TransportError transportErrorOf(const CURLcode code, const ResponseSink &sink)
        {
            if (code == CURLE_WRITE_ERROR && sink.limitExceeded)
                return TransportError{ .message = std::format("The response is larger than the limit of {} bytes", sink.limit) };

            return TransportError{ .message = curl_easy_strerror(code), .cancelled = code == CURLE_ABORTED_BY_CALLBACK };
        }

        void applyMethod(CURL *curl, const Method method)
        {
            switch (method)
            {
                using enum Method;
                case Get:
                    return;
                case Post:
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
                    return;
                case Put:
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                    return;
                case Patch:
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                    return;
                case Delete:
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            }
        }

        void applyRawBody(CURL *curl, const Bytes &body)
        {
            const char *data = body.empty() ? "" : reinterpret_cast<const char *>(body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
        }

        void applyMultipartFile(CurlTransfer &transfer, const MultipartFile &file)
        {
            curl_mime *mime = transfer.createMime();
            curl_mimepart *part = curl_mime_addpart(mime);
            curl_mime_name(part, file.fieldName.c_str());
            curl_mime_filename(part, file.filename.c_str());
            curl_mime_type(part, file.mimeType.c_str());
            curl_mime_data(part, reinterpret_cast<const char *>(file.content.data()), file.content.size());
            curl_easy_setopt(transfer.handle(), CURLOPT_MIMEPOST, mime);
        }

        void applyBody(CurlTransfer &transfer, const RequestBody &body)
        {
            if (const auto *raw = std::get_if<Bytes>(&body))
                applyRawBody(transfer.handle(), *raw);
            else if (const auto *file = std::get_if<MultipartFile>(&body))
                applyMultipartFile(transfer, *file);
        }
    }

    CurlHttpClient::CurlHttpClient(UserAgentProvider userAgent) :
        m_userAgent(std::move(userAgent))
    {
    }

    SendResult CurlHttpClient::send(const Request &request) const
    {
        CurlTransfer transfer;
        CURL *curl = transfer.handle();
        if (curl == nullptr)
            return Failure(TransportError{"Cannot initialize CURL!"});

        for (const auto &[name, value]: request.headers)
            transfer.addHeader(name + ": " + value);

        ResponseSink sink;
        if (request.maxResponseBytes)
            sink.limit = *request.maxResponseBytes;

        curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, transfer.headers());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collectBody);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
        ResponseHeaders headers;
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, collectHeader);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        applyTimeouts(curl, request);
        applyUserAgent(curl, m_userAgent);
        applySecurityOptions(curl);
        applyProxyOptions(curl, request.url);
        applyMethod(curl, request.method);
        applyBody(transfer, request.body);
        applyProgress(curl, request);

        if (const auto result = curl_easy_perform(curl); result != CURLE_OK)
            return Failure(transportErrorOf(result, sink));

        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        std::optional<std::chrono::seconds> retryAfter;
        if (const auto found = headers.values.find("retry-after"); found != headers.values.end())
            retryAfter = parseRetryAfter(found->second, time::systemClock().now());

        return Response{ .status = static_cast<int>(status), .body = std::move(sink.body), .retryAfter = retryAfter, .headers = std::move(headers.values) };
    }
}
