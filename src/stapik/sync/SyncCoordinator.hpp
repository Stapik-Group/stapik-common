#pragma once

#include "SyncEnvelope.hpp"
#include "SyncOutcome.hpp"

#include "stapik/cloud/CloudStorageException.hpp"
#include "stapik/cloud/ICloudStorage.hpp"
#include "stapik/document/DocumentFile.hpp"
#include "stapik/log/Log.hpp"

#include <chrono>
#include <concepts>
#include <exception>
#include <optional>
#include <utility>

namespace stapik::sync
{
    template<typename DocumentType>
    concept SyncableDocument = document::PersistableDocument<DocumentType> && requires(const DocumentType& document)
    {
        { document.lastUpdate() } -> std::same_as<std::chrono::system_clock::time_point>;
        { document.lastKnownCloudUpdate() } -> std::same_as<std::optional<std::chrono::system_clock::time_point>>;
        { document.withLastKnownCloudUpdate(std::chrono::system_clock::time_point{}) } -> std::same_as<DocumentType>;
    };

    template<SyncableDocument DocumentType>
    class SyncCoordinator
    {
    public:
        explicit SyncCoordinator(ICloudStorage& cloudStorage) :
            m_cloudStorage(cloudStorage)
        {}

        [[nodiscard]] SyncOutcome<DocumentType> resolveOnConnect(const DocumentType& localDocument) const
        {
            try
            {
                const auto cloudDocument = m_cloudStorage.loadDocument();

                if (!cloudDocument || isEmptyContent(cloudDocument->content))
                    return upload(localDocument, cloudDocument ? cloudDocument->updatedAt : TimePoint{});

                const auto remote = parseRemote(*cloudDocument);
                if (!remote)
                    return remoteUnreadable(localDocument);

                const auto localTime = floorToSeconds(localDocument.lastUpdate());

                if (remote->contentTime > localTime)
                    return { SyncState::ServerWon, remote->document.withLastKnownCloudUpdate(remote->cloudUpdatedAt) };

                if (remote->contentTime == localTime)
                    return { SyncState::Synchronized, localDocument.withLastKnownCloudUpdate(remote->cloudUpdatedAt) };

                return upload(localDocument, remote->cloudUpdatedAt);
            }
            catch (const CloudStorageException& exception)
            {
                return offline(localDocument, exception);
            }
        }

        [[nodiscard]] SyncOutcome<DocumentType> pushLocalChange(const DocumentType& localDocument) const
        {
            try
            {
                return upload(localDocument, localDocument.lastKnownCloudUpdate().value_or(TimePoint{}));
            }
            catch (const CloudStorageException& exception)
            {
                return offline(localDocument, exception);
            }
        }

    private:
        using TimePoint = std::chrono::system_clock::time_point;

        struct RemoteDocument
        {
            DocumentType document;
            TimePoint contentTime;
            TimePoint cloudUpdatedAt;
        };

        static TimePoint floorToSeconds(const TimePoint timePoint)
        {
            return std::chrono::floor<std::chrono::seconds>(timePoint);
        }

        static bool isEmptyContent(const nlohmann::json& content)
        {
            return content.is_null() || ((content.is_object() || content.is_array()) && content.empty());
        }

        static std::optional<RemoteDocument> parseRemote(const CloudDocument& cloudDocument)
        {
            try
            {
                const auto [lastUpdate, payload] = SyncEnvelope::fromJson(cloudDocument.content);
                return RemoteDocument{
                    DocumentType::fromJson(payload),
                    floorToSeconds(lastUpdate),
                    cloudDocument.updatedAt
                };
            }
            catch (const std::exception& exception)
            {
                log::warning("Cannot read the cloud document: {}", exception.what());
                return std::nullopt;
            }
        }

        static SyncOutcome<DocumentType> offline(const DocumentType& localDocument, const CloudStorageException& exception)
        {
            log::info("Cloud unavailable, keeping the local document: {}", exception.what());
            return { SyncState::OfflineKeptLocal, localDocument, exception.what() };
        }

        static SyncOutcome<DocumentType> remoteUnreadable(const DocumentType& localDocument)
        {
            return { SyncState::RemoteUnreadable, localDocument, "The cloud document could not be read" };
        }

        SyncOutcome<DocumentType> upload(const DocumentType& localDocument, TimePoint baseline) const
        {
            const nlohmann::json content = SyncEnvelope{ .lastUpdate = localDocument.lastUpdate(), .payload = localDocument.toJson() }.toJson();
            const auto localTime = floorToSeconds(localDocument.lastUpdate());

            for (int attempt = 1;; ++attempt)
            {
                constexpr int MAX_ATTEMPTS = 2;
                const auto [document, conflict] = m_cloudStorage.saveDocument(content, baseline);

                if (!conflict)
                    return { SyncState::Synchronized, localDocument.withLastKnownCloudUpdate(document.updatedAt) };

                const bool canRetry = attempt < MAX_ATTEMPTS;

                if (isEmptyContent(document.content))
                {
                    if (!canRetry)
                        return remoteUnreadable(localDocument);

                    baseline = document.updatedAt;
                    continue;
                }

                const auto remote = parseRemote(document);
                if (!remote)
                    return remoteUnreadable(localDocument);

                if (const bool localStillNewer = localTime > remote->contentTime; !localStillNewer || !canRetry)
                {
                    return { SyncState::LostRaceAcceptedServer, remote->document.withLastKnownCloudUpdate(remote->cloudUpdatedAt) };
                }

                baseline = remote->cloudUpdatedAt;
            }
        }

        ICloudStorage& m_cloudStorage;
    };
}
