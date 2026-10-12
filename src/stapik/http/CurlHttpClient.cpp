#include "CurlHttpClient.hpp"
#include "CurlSecurity.hpp"

#include <curl/curl.h>

#include <limits>
#include <utility>

namespace stapik::http
{
    namespace
    {
        class CurlTransfer
        {
        public:
            CurlTransfer() : m_handle(curl_easy_init())
            {
            }

            ~CurlTransfer()
            {
                curl_easy_cleanup(m_handle);
                curl_mime_free(m_mime);
                curl_slist_free_all(m_headers);
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
        };

        std::size_t collectBody(const char *data, const std::size_t size, const std::size_t count, void *destination)
        {
            auto &sink = *static_cast<ResponseSink *>(destination);
            const auto length = size * count;

            if (length > sink.limit - sink.body.size())
                return 0;

            const auto *bytes = reinterpret_cast<const std::uint8_t *>(data);
            sink.body.insert(sink.body.end(), bytes, bytes + length);
            return length;
        }

        void applyMethod(CURL *curl, const Method method)
        {
            switch (method)
            {
                using enum Method;
                case Get:
                    return;
                case Put:
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                    return;
                case Delete:
                    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                    return;
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(request.timeout.count()));
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        applySecurityOptions(curl);
        applyMethod(curl, request.method);
        applyBody(transfer, request.body);

        if (const auto result = curl_easy_perform(curl); result != CURLE_OK)
            return Failure(TransportError{curl_easy_strerror(result)});

        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        return Response{.status = static_cast<int>(status), .body = std::move(sink.body)};
    }

    const IHttpClient &defaultHttpClient()
    {
        static const CurlHttpClient client;
        return client;
    }
}
