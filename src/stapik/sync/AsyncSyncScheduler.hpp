#pragma once

#include "Backoff.hpp"
#include "SyncCoordinator.hpp"
#include "SyncStatus.hpp"

#include "stapik/task/BackgroundTaskRunner.hpp"
#include "stapik/task/DebouncedAction.hpp"

#include <sigc++/signal.h>

#include <chrono>
#include <optional>
#include <utility>

namespace stapik::sync
{
    struct AsyncSyncOptions
    {
        std::chrono::milliseconds debounceDelay{ 1500 };
        std::chrono::milliseconds initialRetryDelay{ 2000 };
        std::chrono::milliseconds maxRetryDelay{ 300000 };
    };

    template<SyncableDocument DocumentType>
    class AsyncSyncScheduler
    {
    public:
        explicit AsyncSyncScheduler(ICloudStorage& cloudStorage, AsyncSyncOptions options = {}) :
            m_options(options),
            m_coordinator(cloudStorage),
            m_debounce(options.debounceDelay, [this] { startJob(); }),
            m_retry(options.initialRetryDelay, [this] { startJob(); })
        {}

        AsyncSyncScheduler(const AsyncSyncScheduler&) = delete;
        AsyncSyncScheduler& operator=(const AsyncSyncScheduler&) = delete;

        void documentChanged(const DocumentType& document)
        {
            if (m_job)
                m_job->document = document;
            else
                m_job = Job{ JobKind::Push, document };

            m_retry.cancel();
            m_debounce.trigger();
        }

        void syncNow(const DocumentType& document)
        {
            m_job = Job{ JobKind::Resolve, document };
            m_debounce.cancel();
            m_retry.cancel();
            startJob();
        }

        void flush()
        {
            m_retry.cancel();

            if (m_debounce.pending())
                m_debounce.flush();
            else
                startJob();
        }

        [[nodiscard]] SyncStatus status() const
        {
            return m_status;
        }

        [[nodiscard]] bool hasPendingWork() const
        {
            return m_inFlight || m_job.has_value();
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
        enum class JobKind
        {
            Push,
            Resolve
        };

        struct Job
        {
            JobKind kind;
            DocumentType document;
        };

        void startJob()
        {
            if (m_inFlight || !m_job)
                return;

            auto job = std::move(*m_job);
            m_job.reset();
            m_inFlight = true;
            m_runningKind = job.kind;
            setStatus(SyncStatus::Syncing);

            m_runner.submit(
                [coordinator = &m_coordinator, job = std::move(job)]
                {
                    return job.kind == JobKind::Resolve
                        ? coordinator->resolveOnConnect(job.document)
                        : coordinator->pushLocalChange(job.document);
                },
                [this](std::optional<SyncOutcome<DocumentType>> result)
                {
                    onJobFinished(std::move(result));
                });
        }

        void onJobFinished(std::optional<SyncOutcome<DocumentType>> result)
        {
            m_inFlight = false;

            if (!result)
            {
                setStatus(SyncStatus::Error);
                return;
            }

            const auto& outcome = *result;
            m_signalOutcome.emit(outcome);

            switch (outcome.state)
            {
                case SyncState::Synchronized:
                    m_consecutiveFailures = 0;
                    rebaseQueuedJob(outcome.document);
                    break;
                case SyncState::ServerWon:
                case SyncState::LostRaceAcceptedServer:
                    m_consecutiveFailures = 0;
                    m_job.reset();
                    m_debounce.cancel();
                    break;
                case SyncState::OfflineKeptLocal:
                    requeueForRetry(outcome.document);
                    break;
                case SyncState::RemoteUnreadable:
                    break;
            }

            setStatus(statusFor(outcome.state));

            const bool canContinue = outcome.state == SyncState::Synchronized;
            if (canContinue && m_job && !m_debounce.pending())
                startJob();
        }

        void rebaseQueuedJob(const DocumentType& synchronized)
        {
            const auto baseline = synchronized.lastKnownCloudUpdate();
            if (m_job && baseline)
                m_job->document = m_job->document.withLastKnownCloudUpdate(*baseline);
        }

        void requeueForRetry(const DocumentType& keptLocal)
        {
            if (!m_job)
                m_job = Job{ m_runningKind, keptLocal };
            else if (m_runningKind == JobKind::Resolve)
                m_job->kind = JobKind::Resolve;

            ++m_consecutiveFailures;

            if (m_debounce.pending())
                return;

            m_retry.setDelay(backoffDelay(m_options.initialRetryDelay, m_options.maxRetryDelay, m_consecutiveFailures));
            m_retry.trigger();
        }

        void setStatus(const SyncStatus status)
        {
            if (status == m_status)
                return;

            m_status = status;
            m_signalStatusChanged.emit(status);
        }

        AsyncSyncOptions m_options;
        SyncCoordinator<DocumentType> m_coordinator;
        task::DebouncedAction m_debounce;
        task::DebouncedAction m_retry;
        std::optional<Job> m_job;
        bool m_inFlight = false;
        JobKind m_runningKind = JobKind::Push;
        int m_consecutiveFailures = 0;
        SyncStatus m_status = SyncStatus::Idle;
        sigc::signal<void(SyncStatus)> m_signalStatusChanged;
        sigc::signal<void(const SyncOutcome<DocumentType>&)> m_signalOutcome;
        task::BackgroundTaskRunner m_runner;
    };
}
