#include "CloudAssetClient.hpp"
#include "CloudAssetProtocol.hpp"
#include "CloudCurl.hpp"

#include "stapik/log/Log.hpp"

#include <curl/curl.h>

#include <format>
#include <utility>

namespace stapik::cloud
{
    namespace
    {
        constexpr long TIMEOUT_SECONDS = 30L;
        constexpr std::size_t MAX_RESPONSE_BYTES = 64U * 1024U * 1024U;
        constexpr int HTTP_OK = 200;
        constexpr int HTTP_CREATED = 201;
        constexpr int HTTP_NO_CONTENT = 204;
        constexpr int HTTP_NOT_FOUND = 404;
        constexpr int HTTP_PAYLOAD_TOO_LARGE = 413;

        class CurlRequest
        {
        public:
            CurlRequest() :
                m_handle(curl_easy_init())
            {}

            ~CurlRequest()
            {
                curl_easy_cleanup(m_handle);
                curl_mime_free(m_mime);
                curl_slist_free_all(m_headers);
            }

            CurlRequest(const CurlRequest&) = delete;
            CurlRequest& operator=(const CurlRequest&) = delete;

            [[nodiscard]] CURL* handle() const { return m_handle; }

            void addHeader(const std::string& header)
            {
                m_headers = curl_slist_append(m_headers, header.c_str());
            }

            [[nodiscard]] curl_slist* headers() const { return m_headers; }

            [[nodiscard]] curl_mime* createMime()
            {
                m_mime = curl_mime_init(m_handle);
                return m_mime;
            }

        private:
            CURL* m_handle;
            curl_slist* m_headers = nullptr;
            curl_mime* m_mime = nullptr;
        };

        std::size_t writeCallback(char* data, const std::size_t size, const std::size_t count, void* destination)
        {
            auto* body = static_cast<AssetBytes*>(destination);
            const auto length = size * count;

            // Returning less than the received length makes libcurl abort the transfer.
            if (body->size() + length > MAX_RESPONSE_BYTES)
                return 0;

            const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
            body->insert(body->end(), bytes, bytes + length);
            return length;
        }
    }

    CloudAssetClient::CloudAssetClient(CloudStorageConfig config, std::string slotKey) :
        m_config(std::move(config)),
        m_slotKey(std::move(slotKey))
    {
        if (m_config.isConfigured() && !m_config.isSecure())
            log::warning("Cloud API URL does not use https:// - the API key and files are sent unencrypted. This is not a secure connection.");
    }

    std::string CloudAssetClient::assetsUrl() const
    {
        return cloud::assetsUrl(m_config.normalizedApiUrl(), m_slotKey);
    }

    std::string CloudAssetClient::assetUrl(const std::string& filename) const
    {
        return cloud::assetUrl(m_config.normalizedApiUrl(), m_slotKey, filename);
    }

    CloudAssetClient::RawResponse CloudAssetClient::perform(const std::string& url, const char* method, const FilePart* file, const char* failureLabel) const
    {
        CurlRequest request;
        if (request.handle() == nullptr)
            throw CloudStorageException("Cannot initialize CURL!");

        request.addHeader("x-api-key: " + m_config.apiKey);

        AssetBytes response;
        CURL* curl = request.handle();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        if (method != nullptr)
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request.headers());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SECONDS);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        applySecurityOptions(curl);

        if (file != nullptr)
        {
            curl_mime* mime = request.createMime();
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, "file");
            curl_mime_filename(part, file->filename.c_str());
            curl_mime_type(part, file->mimeType.c_str());
            curl_mime_data(part, reinterpret_cast<const char*>(file->content.data()), file->content.size());
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        }

        const auto result = curl_easy_perform(curl);

        long httpStatus = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

        if (result != CURLE_OK)
            throw CloudStorageException(std::string(failureLabel) + curl_easy_strerror(result));

        return { .httpStatus = httpStatus, .body = std::move(response) };
    }

    std::vector<AssetInfo> CloudAssetClient::listAssets() const
    {
        if (!m_config.isConfigured())
            return {};

        const auto [httpStatus, body] = perform(assetsUrl(), nullptr, nullptr, "API asset list failed: ");

        if (httpStatus == HTTP_NOT_FOUND)
            return {};

        if (httpStatus != HTTP_OK)
            throw CloudStorageException(std::format("API asset list failed with status {}", httpStatus));

        return parseAssetList(std::string(body.begin(), body.end()));
    }

    std::optional<AssetBytes> CloudAssetClient::downloadAsset(const std::string& filename) const
    {
        if (!m_config.isConfigured())
            return std::nullopt;

        auto [httpStatus, body] = perform(assetUrl(filename), nullptr, nullptr, "API asset download failed: ");

        if (httpStatus == HTTP_NOT_FOUND)
            return std::nullopt;

        if (httpStatus != HTTP_OK)
            throw CloudStorageException(std::format("API asset download failed with status {}", httpStatus));

        return std::move(body);
    }

    AssetInfo CloudAssetClient::uploadAsset(const std::string& filename, const std::string& mimeType, const AssetBytes& content) const
    {
        if (!m_config.isConfigured())
            throw CloudStorageException("Cloud sync is not configured");

        const FilePart file{ .filename = filename, .mimeType = mimeType, .content = content };
        const auto [httpStatus, body] = perform(assetUrl(filename), "PUT", &file, "API asset upload failed: ");

        if (httpStatus == HTTP_PAYLOAD_TOO_LARGE)
            throw AssetTooLargeException(std::format("File '{}' is too large for the cloud slot", filename));

        if (httpStatus != HTTP_OK && httpStatus != HTTP_CREATED)
            throw CloudStorageException(std::format("API asset upload failed with status {}", httpStatus));

        return parseAssetMetadata(std::string(body.begin(), body.end()));
    }

    bool CloudAssetClient::deleteAsset(const std::string& filename) const
    {
        if (!m_config.isConfigured())
            return false;

        const auto [httpStatus, body] = perform(assetUrl(filename), "DELETE", nullptr, "API asset delete failed: ");

        if (httpStatus == HTTP_NO_CONTENT)
            return true;

        if (httpStatus == HTTP_NOT_FOUND)
            return false;

        throw CloudStorageException(std::format("API asset delete failed with status {}", httpStatus));
    }
}
