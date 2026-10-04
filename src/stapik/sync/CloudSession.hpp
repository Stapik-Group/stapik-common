#pragma once

#include "AsyncSyncScheduler.hpp"
#include "CloudSessionHooks.hpp"
#include "PartitionedSyncService.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/task/BackgroundTaskRunner.hpp"

#include <sigc++/signal.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

            for (auto& [key, entry] : m_partitions)
                entry.scheduler->flush();
        }

        void loadPartition(const std::string& partition, const DocumentType& currentDocument)
        {
            if (!m_storage)
                return;

            auto& entry = partitionEntry(partition);
            if (entry.loaded || entry.scheduler->hasPendingWork())
                return;

            entry.scheduler->syncNow(currentDocument);
        }

        void pushPartitionChange(const std::string& partition, const DocumentType& currentDocument)
        {
            if (m_storage)
                partitionEntry(partition).scheduler->documentChanged(currentDocument);
        }

        void forgetPartition(const std::string& partition)
        {
            if (const auto found = m_partitions.find(partition); found != m_partitions.end())
                found->second.loaded = false;
        }

        void fetchAvailableYears()
        {
            if (!m_storage)
                return;

            if (!m_yearsRunner)
                m_yearsRunner = std::make_unique<task::BackgroundTaskRunner>();

            m_yearsRunner->submit(
                [storage = m_storage.get()]
                {
                    return PartitionedSyncService<DocumentType>(*storage).availableYears();
                },
                [this](std::optional<std::optional<std::vector<int>>> result)
                {
                    std::optional<std::vector<int>> years;
                    if (result)
                        years = std::move(*result);

                    m_signalAvailableYears.emit(years);
                });
        }

        [[nodiscard]] bool isPartitionLoaded(const std::string& partition) const
        {
            const auto found = m_partitions.find(partition);
            return found != m_partitions.end() && found->second.loaded;
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
            if (m_scheduler && m_scheduler->hasPendingWork())
                return true;

            return std::ranges::any_of(m_partitions, [](const auto& item) { return item.second.scheduler->hasPendingWork(); });
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

        sigc::signal<void(const std::string&, const DocumentType&)>& signalPartitionReplaced()
        {
            return m_signalPartitionReplaced;
        }

        sigc::signal<void(const std::string&, TimePoint)>& signalPartitionBaselineChanged()
        {
            return m_signalPartitionBaselineChanged;
        }

        sigc::signal<void(const std::string&, SyncStatus)>& signalPartitionStatusChanged()
        {
            return m_signalPartitionStatusChanged;
        }

        sigc::signal<void(const std::string&, const SyncOutcome<DocumentType>&)>& signalPartitionOutcome()
        {
            return m_signalPartitionOutcome;
        }

        sigc::signal<void(const std::optional<std::vector<int>>&)>& signalAvailableYears()
        {
            return m_signalAvailableYears;
        }

    private:
        struct PartitionEntry
        {
            std::unique_ptr<AsyncSyncScheduler<DocumentType>> scheduler;
            bool loaded = false;
        };

        PartitionEntry& partitionEntry(const std::string& partition)
        {
            if (const auto found = m_partitions.find(partition); found != m_partitions.end())
                return found->second;

            auto scheduler = std::make_unique<AsyncSyncScheduler<DocumentType>>(*m_storage, m_options, partition);

            scheduler->signalStatusChanged().connect([this, partition](const SyncStatus status)
            {
                m_signalPartitionStatusChanged.emit(partition, status);
            });

            scheduler->signalOutcome().connect([this, partition](const SyncOutcome<DocumentType>& outcome)
            {
                onPartitionOutcome(partition, outcome);
            });

            return m_partitions.emplace(partition, PartitionEntry{ .scheduler = std::move(scheduler) }).first->second;
        }

        void onPartitionOutcome(const std::string& partition, const SyncOutcome<DocumentType>& outcome)
        {
            if (const auto found = m_partitions.find(partition); found != m_partitions.end()
                && (outcome.state == SyncState::Synchronized || outcome.replacesLocal()))
            {
                found->second.loaded = true;
            }

            m_signalPartitionOutcome.emit(partition, outcome);

            if (outcome.replacesLocal())
            {
                m_signalPartitionReplaced.emit(partition, outcome.document);
                return;
            }

            const auto baseline = outcome.document.lastKnownCloudUpdate();
            if (outcome.state == SyncState::Synchronized && baseline)
                m_signalPartitionBaselineChanged.emit(partition, *baseline);
        }

        bool activate(const CloudStorageConfig& config)
        {
            m_partitions.clear();
            m_yearsRunner.reset();
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
        std::map<std::string, PartitionEntry> m_partitions;
        std::unique_ptr<task::BackgroundTaskRunner> m_yearsRunner;
        sigc::signal<void(const DocumentType&)> m_signalDocumentReplaced;
        sigc::signal<void(TimePoint)> m_signalBaselineChanged;
        sigc::signal<void(SyncStatus)> m_signalStatusChanged;
        sigc::signal<void(const SyncOutcome<DocumentType>&)> m_signalOutcome;
        sigc::signal<void(const std::string&, const DocumentType&)> m_signalPartitionReplaced;
        sigc::signal<void(const std::string&, TimePoint)> m_signalPartitionBaselineChanged;
        sigc::signal<void(const std::string&, SyncStatus)> m_signalPartitionStatusChanged;
        sigc::signal<void(const std::string&, const SyncOutcome<DocumentType>&)> m_signalPartitionOutcome;
        sigc::signal<void(const std::optional<std::vector<int>>&)> m_signalAvailableYears;
    };
}
