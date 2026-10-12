#pragma once

#include "stapik/http/IHttpClient.hpp"

#include <chrono>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace stapik::test
{
    class FakeHttpClient final : public http::IHttpClient
    {
    public:
        mutable std::vector<http::Request> requests;
        std::vector<http::TransferProgress> progressReports;

        void respondWith(const int status, const std::string& body = {}) const
        {
            respondWith(status, http::toBytes(body));
        }

        void respondWith(const int status, http::Bytes body) const
        {
            m_replies.emplace_back(http::Response{ .status = status, .body = std::move(body) });
        }

        void respondWithRetryAfter(const int status, const std::chrono::seconds retryAfter) const
        {
            m_replies.emplace_back(http::Response{ .status = status, .retryAfter = retryAfter });
        }

        void failWith(const std::string& message) const
        {
            m_replies.emplace_back(Failure(http::TransportError{ message }));
        }

        [[nodiscard]] http::SendResult send(const http::Request& request) const override
        {
            requests.push_back(request);

            for (const auto& progress : progressReports)
            {
                if (request.onProgress)
                    request.onProgress(progress);

                if (request.isCancelled && request.isCancelled())
                    return Failure(http::TransportError{ .message = "Transfer cancelled", .cancelled = true });
            }

            if (m_replies.empty())
                return Failure(http::TransportError{ "no response queued" });

            auto reply = std::move(m_replies.front());
            m_replies.pop_front();
            return reply;
        }

    private:
        mutable std::deque<http::SendResult> m_replies;
    };

    [[nodiscard]] inline std::string bodyText(const http::Request& request)
    {
        return http::toText(std::get<http::Bytes>(request.body));
    }
}
