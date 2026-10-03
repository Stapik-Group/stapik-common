#pragma once

#include "AsyncSyncScheduler.hpp"
#include "CloudSessionHooks.hpp"

#include "stapik/log/Log.hpp"

#include <sigc++/signal.h>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

namespace stapik::sync
{
    template<SyncableDocument DocumentType>
    class CloudSession
    {
    public:
        using TimePoint = std::chrono::system_clock::time_point;

        explicit CloudSession(CloudSessionHooks hooks, const AsyncSyncOptions &options = {}) :
            m_hooks(std::move(hooks)),
            m_options(options)
        {
            if (!m_hooks.loadConfig)
                return;

            if (const auto config = m_hooks.loadConfig(); config && config->isConfigured())
                activate(*config);
        }

        CloudSession(const CloudSession&) = delete;
        CloudSession& operator=(const CloudSession&) = delete;

        bool connectWith(const CloudStorageConfig& config, const DocumentType& currentDocument)
        {
            if (!config.isConfigured())
                return false;

            if (m_hooks.saveConfig && !m_hooks.saveConfig(config))
                log::warning("Cannot save the cloud configuration; it will be lost after restart");

            if (!activate(config))
                return false;

            m_scheduler->syncNow(currentDocument);
            return true;
        }

        void syncNow(const DocumentType& currentDocument)
        {
            if (m_scheduler)
                m_scheduler->syncNow(currentDocument);
        }

        void pushChange(const DocumentType& currentDocument)
        {
            if (m_scheduler)
                m_scheduler->documentChanged(currentDocument);
        }

        void flush()
        {
            if (m_scheduler)
                m_scheduler->flush();
        }

        [[nodiscard]] bool isConnected() const
        {
            return m_scheduler != nullptr;
        }

        [[nodiscard]] const std::optional<CloudStorageConfig>& config() const
        {
            return m_config;
        }

        [[nodiscard]] SyncStatus status() const
        {
            return m_scheduler ? m_scheduler->status() : SyncStatus::Idle;
        }

        [[nodiscard]] bool hasPendingWork() const
        {
            return m_scheduler && m_scheduler->hasPendingWork();
        }

        sigc::signal<void(const DocumentType&)>& signalDocumentReplaced()
        {
            return m_signalDocumentReplaced;
        }

        sigc::signal<void(TimePoint)>& signalBaselineChanged()
        {
            return m_signalBaselineChanged;
        }

        sigc::signal<void(SyncStatus)>& signalStatusChanged()
        {
            return m_signalStatusChanged;
        }

        sigc::signal<void(const SyncOutcome<DocumentType>&)>& signalOutcome()
        {
            return m_signalOutcome;
        }

    private:
        bool activate(const CloudStorageConfig& config)
        {
            m_scheduler.reset();
            m_storage.reset();
            m_config.reset();

            if (!m_hooks.createStorage)
                return false;

            m_storage = m_hooks.createStorage(config);
            if (!m_storage)
                return false;

            m_config = config;
            m_scheduler = std::make_unique<AsyncSyncScheduler<DocumentType>>(*m_storage, m_options);

            m_scheduler->signalStatusChanged().connect([this](const SyncStatus status)
            {
                m_signalStatusChanged.emit(status);
            });

            m_scheduler->signalOutcome().connect([this](const SyncOutcome<DocumentType>& outcome)
            {
                onOutcome(outcome);
            });

            m_signalStatusChanged.emit(SyncStatus::Idle);
            return true;
        }

        void onOutcome(const SyncOutcome<DocumentType>& outcome)
        {
            m_signalOutcome.emit(outcome);

            if (outcome.replacesLocal())
            {
                m_signalDocumentReplaced.emit(outcome.document);
                return;
            }

            const auto baseline = outcome.document.lastKnownCloudUpdate();
            if (outcome.state == SyncState::Synchronized && baseline)
                m_signalBaselineChanged.emit(*baseline);
        }

        CloudSessionHooks m_hooks;
        AsyncSyncOptions m_options;
        std::optional<CloudStorageConfig> m_config;
        std::unique_ptr<ICloudStorage> m_storage;
        std::unique_ptr<AsyncSyncScheduler<DocumentType>> m_scheduler;
        sigc::signal<void(const DocumentType&)> m_signalDocumentReplaced;
        sigc::signal<void(TimePoint)> m_signalBaselineChanged;
        sigc::signal<void(SyncStatus)> m_signalStatusChanged;
        sigc::signal<void(const SyncOutcome<DocumentType>&)> m_signalOutcome;
    };
}
