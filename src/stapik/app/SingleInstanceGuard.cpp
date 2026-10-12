#include "SingleInstanceGuard.hpp"

#include "InstanceMessage.hpp"

#include "stapik/locale/LocaleManager.hpp"
#include "stapik/log/Log.hpp"
#include "stapik/task/MainThread.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <optional>
#include <thread>
#include <tuple>

#ifndef PIPE_REJECT_REMOTE_CLIENTS
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008
#endif
#endif

namespace stapik::app
{
#ifdef _WIN32
    namespace
    {
        constexpr DWORD PIPE_BUFFER_BYTES = 64 * 1024;
        constexpr std::size_t MAX_MESSAGE_BYTES = 256 * 1024;
        constexpr DWORD IO_TIMEOUT_MS = 2000;
        constexpr DWORD RETRY_DELAY_MS = 100;
        constexpr char ACK = 1;

        std::wstring toWide(const std::string &utf8)
        {
            if (utf8.empty())
                return {};

            const auto length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (length <= 0)
                return {};

            std::wstring wide(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
            return wide;
        }

        // Pipe names are machine-wide, so the session id keeps the pipe in step with the session-local mutex.
        std::wstring pipeName(const std::string &applicationId)
        {
            DWORD session = 0;
            ProcessIdToSessionId(GetCurrentProcessId(), &session);
            return L"\\\\.\\pipe\\" + toWide(applicationId) + L"." + std::to_wstring(session);
        }

        // Sends the files to the first instance and waits for its acknowledgement.
        bool forwardToRunningInstance(const std::wstring &name, const std::string &message)
        {
            // The first instance can only come to the front if this process, which the user just started, allows it.
            AllowSetForegroundWindow(ASFW_ANY);

            char reply = 0;
            DWORD received = 0;
            const BOOL done = CallNamedPipeW(name.c_str(), const_cast<char *>(message.data()),
                                             static_cast<DWORD>(message.size()),
                                             &reply, sizeof(reply), &received, IO_TIMEOUT_MS);

            if (!done)
            {
                log::warning("Cannot reach the running instance (error {})", GetLastError());
                return false;
            }
            return received == sizeof(reply) && reply == ACK;
        }

        class PipeServer
        {
        public:
            using Delivery = std::function<void(std::vector<std::filesystem::path>)>;

            [[nodiscard]] static std::unique_ptr<PipeServer> start(const std::wstring &name, Delivery delivery)
            {
                const HANDLE pipe = CreateNamedPipeW(name.c_str(),
                                                     PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                                     FILE_FLAG_FIRST_PIPE_INSTANCE,
                                                     PIPE_TYPE_MESSAGE | PIPE_READ_MODE_MESSAGE | PIPE_WAIT |
                                                     PIPE_REJECT_REMOTE_CLIENTS,
                                                     1, PIPE_BUFFER_BYTES, PIPE_BUFFER_BYTES, 0, nullptr);

                if (pipe == INVALID_HANDLE_VALUE)
                {
                    log::warning("Cannot create the instance pipe (error {}); later instances will only show a message",
                                 GetLastError());
                    return nullptr;
                }

                return std::unique_ptr<PipeServer>(new PipeServer(pipe, std::move(delivery)));
            }

            ~PipeServer()
            {
                SetEvent(m_stopEvent);
                m_thread.join();

                CloseHandle(m_ioEvent);
                CloseHandle(m_stopEvent);
                CloseHandle(m_pipe);
            }

            PipeServer(const PipeServer &) = delete;

            PipeServer &operator=(const PipeServer &) = delete;

        private:
            enum class Step { Done, Stopped, Failed };

            PipeServer(HANDLE pipe, Delivery delivery) : m_pipe(pipe),
                                                         m_stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
                                                         m_ioEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
                                                         m_delivery(std::move(delivery)),
                                                         m_thread([this] { serve(); })
            {
            }

            void serve()
            {
                while (!stopRequested())
                {
                    const auto connected = awaitClient();
                    if (connected == Step::Stopped)
                        return;

                    if (connected == Step::Failed)
                    {
                        DisconnectNamedPipe(m_pipe);
                        if (WaitForSingleObject(m_stopEvent, RETRY_DELAY_MS) == WAIT_OBJECT_0)
                            return;
                        continue;
                    }

                    handleClient();
                    DisconnectNamedPipe(m_pipe);
                }
            }

            void handleClient()
            {
                const auto message = readMessage();
                if (!message)
                    return;

                const auto files = decodeInstanceMessage(*message);
                if (!files)
                {
                    log::warning("Ignoring a malformed message from another instance");
                    return;
                }

                acknowledge();
                m_delivery(*files);
            }

            [[nodiscard]] bool stopRequested() const
            {
                return WaitForSingleObject(m_stopEvent, 0) == WAIT_OBJECT_0;
            }

            [[nodiscard]] OVERLAPPED beginIo() const
            {
                ResetEvent(m_ioEvent);

                OVERLAPPED overlapped{};
                overlapped.hEvent = m_ioEvent;
                return overlapped;
            }

            // Waits for a pending operation, cancelling it when the server is stopped or the time runs out.
            [[nodiscard]] Step awaitIo(OVERLAPPED &overlapped, DWORD timeoutMs) const
            {
                const std::array handles{m_ioEvent, m_stopEvent};
                const auto waited = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE,
                                                           timeoutMs);
                if (waited == WAIT_OBJECT_0)
                    return Step::Done;

                CancelIoEx(m_pipe, &overlapped);
                DWORD ignored = 0;
                GetOverlappedResult(m_pipe, &overlapped, &ignored, TRUE);
                return waited == WAIT_OBJECT_0 + 1 ? Step::Stopped : Step::Failed;
            }

            [[nodiscard]] Step awaitClient() const
            {
                auto overlapped = beginIo();
                if (ConnectNamedPipe(m_pipe, &overlapped))
                    return Step::Done;

                switch (GetLastError())
                {
                    case ERROR_PIPE_CONNECTED:
                        return Step::Done;
                    case ERROR_IO_PENDING:
                        return awaitIo(overlapped, INFINITE);
                    default:
                        return Step::Failed;
                }
            }

            [[nodiscard]] std::optional<std::string> readMessage() const
            {
                std::string message;
                std::array<char, 4096> chunk{};

                while (true)
                {
                    auto overlapped = beginIo();
                    DWORD read = 0;
                    BOOL complete = ReadFile(m_pipe, chunk.data(), static_cast<DWORD>(chunk.size()), &read,
                                             &overlapped);
                    DWORD error = complete ? ERROR_SUCCESS : GetLastError();

                    if (!complete && error == ERROR_IO_PENDING)
                    {
                        if (awaitIo(overlapped, IO_TIMEOUT_MS) != Step::Done)
                            return std::nullopt;

                        complete = GetOverlappedResult(m_pipe, &overlapped, &read, FALSE);
                        error = complete ? ERROR_SUCCESS : GetLastError();
                    }

                    if (!complete && error != ERROR_MORE_DATA)
                        return std::nullopt;

                    message.append(chunk.data(), read);
                    if (message.size() > MAX_MESSAGE_BYTES)
                        return std::nullopt;

                    if (complete)
                        return message;
                }
            }

            void acknowledge() const
            {
                auto overlapped = beginIo();
                DWORD written = 0;
                if (!WriteFile(m_pipe, &ACK, sizeof(ACK), &written, &overlapped) && GetLastError() == ERROR_IO_PENDING)
                    std::ignore = awaitIo(overlapped, IO_TIMEOUT_MS);

                // Disconnecting drops what the client has not read yet.
                FlushFileBuffers(m_pipe);
            }

            HANDLE m_pipe;
            HANDLE m_stopEvent;
            HANDLE m_ioEvent;
            Delivery m_delivery;
            std::thread m_thread;
        };
    }

    struct SingleInstanceGuard::Impl
    {
        HANDLE mutex = nullptr;
        std::shared_ptr<SecondInstanceHandler> handler;
        std::unique_ptr<PipeServer> server;

        ~Impl()
        {
            server.reset();

            // Deliveries already queued for the main thread must not reach a handler of a destroyed application.
            if (handler)
                *handler = nullptr;

            if (mutex != nullptr)
                CloseHandle(mutex);
        }
    };

    std::unique_ptr<SingleInstanceGuard> SingleInstanceGuard::acquire(const std::string &applicationId,
                                                                      const std::string &displayName,
                                                                      const std::vector<std::filesystem::path> &files,
                                                                      SecondInstanceHandler onSecondInstance)
    {
        // "Local\" scopes the name to the user's session: two users on one machine may each run a copy.
        const auto name = L"Local\\" + toWide(applicationId);

        const HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
        const auto mutexError = GetLastError();
        if (mutex == nullptr)
        {
            // Better to start than to refuse to start because of a failure of the guard itself.
            log::warning("Cannot create the single instance mutex (error {}); starting without it", mutexError);
            return std::unique_ptr<SingleInstanceGuard>(new SingleInstanceGuard(nullptr));
        }

        const auto pipe = pipeName(applicationId);

        if (mutexError == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(mutex);
            log::info("{} is already running", displayName);

            if (forwardToRunningInstance(pipe, encodeInstanceMessage(files)))
                return nullptr;

            const auto text = LocaleManager::instance().translate("app.alreadyRunning", {{"name", displayName}});
            MessageBoxW(nullptr, toWide(text).c_str(), toWide(displayName).c_str(),
                        MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
            return nullptr;
        }

        auto impl = std::make_unique<Impl>();
        impl->mutex = mutex;
        impl->handler = std::make_shared<SecondInstanceHandler>(std::move(onSecondInstance));
        impl->server = PipeServer::start(pipe, [handler = impl->handler](std::vector<std::filesystem::path> forwarded)
        {
            task::postToMainThread([handler, forwarded = std::move(forwarded)]
            {
                if (*handler)
                    (*handler)(forwarded);
            });
        });

        return std::unique_ptr<SingleInstanceGuard>(new SingleInstanceGuard(std::move(impl)));
    }
#else
    struct SingleInstanceGuard::Impl
    {
    };

    std::unique_ptr<SingleInstanceGuard> SingleInstanceGuard::acquire(const std::string &, const std::string &,
                                                                      const std::vector<std::filesystem::path> &,
                                                                      SecondInstanceHandler)
    {
        return std::unique_ptr<SingleInstanceGuard>(new SingleInstanceGuard(nullptr));
    }
#endif

    SingleInstanceGuard::SingleInstanceGuard(std::unique_ptr<Impl> impl) : m_impl(std::move(impl))
    {
    }

    SingleInstanceGuard::~SingleInstanceGuard() = default;
}
