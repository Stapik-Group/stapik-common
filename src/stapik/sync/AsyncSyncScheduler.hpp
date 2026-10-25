#pragma once

#include "Backoff.hpp"
#include "SyncCoordinator.hpp"
#include "SyncStatus.hpp"

#include "stapik/log/Log.hpp"
#include "stapik/task/BackgroundTaskRunner.hpp"
#include "stapik/task/DebouncedAction.hpp"

#include <sigc++/signal.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
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
        explicit AsyncSyncScheduler(ICloudStorage& cloudStorage, AsyncSyncOptions options = {}, std::optional<std::string> partition = std::nullopt) :
            m_options(options),
            m_coordinator(cloudStorage, std::move(partition)),
            m_debounce(options.debounceDelay, [this] { startJob(); }),
            m_retry(options.initialRetryDelay, [this] { startJob(); })
        {}

        AsyncSyncScheduler(const AsyncSyncScheduler&) = delete;
        AsyncSyncScheduler& operator=(const AsyncSyncScheduler&) = delete;

        [[nodiscard]] const std::optional<std::string>& partition() const
        {
            return m_coordinator.partition();
        }

        void documentChanged(const DocumentType& document)
        {
            if (m_job)
                m_job->document = document;
            else
                m_job = Job{ .kind = JobKind::Push, .document = document };

            m_retry.cancel();
            m_debounce.trigger();
        }

        void syncNow(const DocumentType& document)
        {
            m_job = Job{ .kind = JobKind::Resolve, .document = document };
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
            m_runningDocument = job.document;
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
            auto runningDocument = std::exchange(m_runningDocument, std::nullopt);

            if (!result)
            {
                onJobFailed(std::move(runningDocument));
                return;
            }

            const auto& outcome = *result;
            m_signalOutcome.emit(outcome);

            switch (outcome.state)
            {
                using enum SyncState;
                case Synchronized:
                    m_consecutiveFailures = 0;
                    rebaseQueuedJob(outcome.document);
                    break;
                case ServerWon:
                case LostRaceAcceptedServer:
                    m_consecutiveFailures = 0;
                    m_job.reset();
                    m_debounce.cancel();
                    break;
                case OfflineKeptLocal:
                    requeueForRetry(outcome.document, outcome.retryAfter);
                    break;
                case RemoteUnreadable:
                default:
                    break;
            }

            setStatus(statusFor(outcome.state));

            const bool canContinue = outcome.state == SyncState::Synchronized;
            if (canContinue && m_job && !m_debounce.pending())
                startJob();
        }

        void onJobFailed(std::optional<DocumentType> failedDocument)
        {
            if (failedDocument)
                requeueForRetry(*failedDocument, std::nullopt);

            log::error("Sync of {} failed unexpectedly, keeping the change for a retry (consecutive failures: {})",
                partition().value_or("the main document"), m_consecutiveFailures);

            setStatus(SyncStatus::Error);
        }

        void rebaseQueuedJob(const DocumentType& synchronized)
        {
            const auto baseline = synchronized.lastKnownCloudUpdate();
            if (m_job && baseline)
                m_job->document = m_job->document.withLastKnownCloudUpdate(*baseline);
        }

        void requeueForRetry(const DocumentType& keptLocal, const std::optional<std::chrono::seconds> retryAfter)
        {
            if (!m_job)
                m_job = Job{ .kind = m_runningKind, .document = keptLocal };
            else if (m_runningKind == JobKind::Resolve)
                m_job->kind = JobKind::Resolve;

            ++m_consecutiveFailures;

            if (m_debounce.pending())
                return;

            auto delay = backoffDelay(m_options.initialRetryDelay, m_options.maxRetryDelay, m_consecutiveFailures);
            if (retryAfter)
                delay = std::max(delay, std::chrono::duration_cast<std::chrono::milliseconds>(*retryAfter));

            m_retry.setDelay(delay);
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
        std::optional<DocumentType> m_runningDocument;
        int m_consecutiveFailures = 0;
        SyncStatus m_status = SyncStatus::Idle;
        sigc::signal<void(SyncStatus)> m_signalStatusChanged;
        sigc::signal<void(const SyncOutcome<DocumentType>&)> m_signalOutcome;
        task::BackgroundTaskRunner m_runner;
    };
}
