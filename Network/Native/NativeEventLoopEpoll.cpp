#include "Network/Native/NativeEventLoop.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>

namespace MoongTangE {
namespace {

std::string system_error(int error)
{
    return std::strerror(error);
}

} // namespace

class NativeEventLoop::Impl
{
  public:
    Impl()
    {
        m_epoll = epoll_create1(EPOLL_CLOEXEC);
        if (m_epoll == -1) {
            throw std::runtime_error(system_error(errno));
        }

        m_wake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (m_wake == -1) {
            const int error = errno;
            close(m_epoll);
            throw std::runtime_error(system_error(error));
        }

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = m_wake;
        if (epoll_ctl(m_epoll, EPOLL_CTL_ADD, m_wake, &event) == -1) {
            const int error = errno;
            close(m_wake);
            close(m_epoll);
            throw std::runtime_error(system_error(error));
        }
    }

    ~Impl()
    {
        for (const auto& [connection, socket] : m_connections) {
            (void)connection;
            close(socket->m_socket);
        }
        if (m_listener != -1) {
            close(m_listener);
        }
        close(m_wake);
        close(m_epoll);
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::uint16_t Listen(const std::string& address, std::uint16_t port, AcceptHandler onAccept)
    {
        if (m_listener != -1 || !onAccept) {
            throw std::logic_error("The native event loop owns one listener");
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICHOST;
        addrinfo* addresses = nullptr;
        const std::string service = std::to_string(port);
        const int resolveError = getaddrinfo(address.c_str(), service.c_str(), &hints, &addresses);
        if (resolveError != 0) {
            throw std::invalid_argument(gai_strerror(resolveError));
        }

        int listener = -1;
        for (auto* candidate = addresses; candidate; candidate = candidate->ai_next) {
            listener =
                socket(candidate->ai_family, candidate->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                       candidate->ai_protocol);
            if (listener == -1) {
                continue;
            }

            constexpr int ENABLED = 1;
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &ENABLED, sizeof(ENABLED));
            if (bind(listener, candidate->ai_addr, candidate->ai_addrlen) == 0 &&
                listen(listener, SOMAXCONN) == 0) {
                break;
            }

            close(listener);
            listener = -1;
        }
        freeaddrinfo(addresses);
        if (listener == -1) {
            throw std::runtime_error(system_error(errno));
        }

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = listener;
        if (epoll_ctl(m_epoll, EPOLL_CTL_ADD, listener, &event) == -1) {
            const int error = errno;
            close(listener);
            throw std::runtime_error(system_error(error));
        }

        sockaddr_storage boundAddress{};
        socklen_t boundSize = sizeof(boundAddress);
        if (getsockname(listener, reinterpret_cast<sockaddr*>(&boundAddress), &boundSize) == -1) {
            const int error = errno;
            close(listener);
            throw std::runtime_error(system_error(error));
        }

        m_listener = listener;
        m_onAccept = std::move(onAccept);
        return ntohs(reinterpret_cast<const sockaddr_in*>(&boundAddress)->sin_port);
    }

    void Connect(const std::string& host, std::uint16_t port, ConnectHandler onConnect)
    {
        if (!onConnect) {
            throw std::invalid_argument("Connect requires a completion handler");
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* addresses = nullptr;
        const std::string service = std::to_string(port);
        const int resolveError = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
        if (resolveError != 0) {
            Queue(
                [handler = std::move(onConnect), error = std::string(gai_strerror(resolveError))] {
                    handler(0, error);
                });
            return;
        }

        int socketHandle = -1;
        bool connecting = false;
        int lastError = ECONNREFUSED;
        for (auto* candidate = addresses; candidate; candidate = candidate->ai_next) {
            socketHandle =
                socket(candidate->ai_family, candidate->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                       candidate->ai_protocol);
            if (socketHandle == -1) {
                lastError = errno;
                continue;
            }

            constexpr int ENABLED = 1;
            setsockopt(socketHandle, IPPROTO_TCP, TCP_NODELAY, &ENABLED, sizeof(ENABLED));
            if (connect(socketHandle, candidate->ai_addr, candidate->ai_addrlen) == 0) {
                connecting = false;
                break;
            }
            if (errno == EINPROGRESS) {
                connecting = true;
                break;
            }

            lastError = errno;
            close(socketHandle);
            socketHandle = -1;
        }
        freeaddrinfo(addresses);
        if (socketHandle == -1) {
            Queue([handler = std::move(onConnect), error = system_error(lastError)] {
                handler(0, error);
            });
            return;
        }

        auto state = std::make_shared<SocketState>();
        state->m_id = m_nextConnection++;
        state->m_socket = socketHandle;
        state->m_connecting = connecting;
        state->m_onConnect = std::move(onConnect);
        m_connections.emplace(state->m_id, state);
        m_socketConnections.emplace(socketHandle, state->m_id);

        AddSocket(state);
        if (!connecting) {
            CompleteConnect(state, {});
        }
    }

    void Read(ConnectionId connection, std::span<std::uint8_t> bytes, IoHandler onComplete)
    {
        auto state = Find(connection);
        if (!state || state->m_read.m_handler || bytes.empty()) {
            Queue([handler = std::move(onComplete)] {
                handler("Invalid native read operation");
            });
            return;
        }

        state->m_read.m_readBytes = bytes.data();
        state->m_read.m_size = bytes.size();
        state->m_read.m_handler = std::move(onComplete);
        ServiceRead(state);
    }

    void Write(ConnectionId connection, std::span<const std::uint8_t> bytes, IoHandler onComplete)
    {
        auto state = Find(connection);
        if (!state || state->m_write.m_handler || bytes.empty()) {
            Queue([handler = std::move(onComplete)] {
                handler("Invalid native write operation");
            });
            return;
        }

        state->m_write.m_writeBytes = bytes.data();
        state->m_write.m_size = bytes.size();
        state->m_write.m_handler = std::move(onComplete);
        UpdateEvents(state);
        ServiceWrite(state);
    }

    void Close(ConnectionId connection)
    {
        auto state = Find(connection);
        if (!state || state->m_closed) {
            return;
        }

        state->m_closed = true;
        epoll_ctl(m_epoll, EPOLL_CTL_DEL, state->m_socket, nullptr);
        shutdown(state->m_socket, SHUT_RDWR);
        close(state->m_socket);
        m_socketConnections.erase(state->m_socket);
        m_connections.erase(connection);

        CompleteIo(state->m_read, "Connection closed");
        CompleteIo(state->m_write, "Connection closed");
        if (state->m_onConnect) {
            CompleteConnect(state, "Connection closed");
        }
    }

    bool Open(ConnectionId connection) const
    {
        const auto found = m_connections.find(connection);
        return found != m_connections.end() && !found->second->m_closed;
    }

    void Post(Task task)
    {
        Queue(std::move(task));
    }

    TimerId Schedule(std::chrono::steady_clock::duration delay, Task task)
    {
        const TimerId timer = m_nextTimer++;
        m_timers.emplace(timer, Timer{std::chrono::steady_clock::now() + delay, std::move(task)});
        Wake();
        return timer;
    }

    void Cancel(TimerId timer)
    {
        if (timer != 0) {
            m_timers.erase(timer);
        }
    }

    void Run()
    {
        std::array<epoll_event, 64> events{};
        while (!m_stopping) {
            DrainTasks();
            RunTimers();
            if (m_stopping) {
                break;
            }

            const int count = epoll_wait(m_epoll, events.data(), static_cast<int>(events.size()),
                                         WaitMilliseconds());
            if (count == -1) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::runtime_error(system_error(errno));
            }

            for (int index = 0; index < count; ++index) {
                const int socketHandle = events[index].data.fd;
                if (socketHandle == m_wake) {
                    DrainWake();
                } else if (socketHandle == m_listener) {
                    AcceptConnections();
                } else {
                    HandleSocketEvent(socketHandle, events[index].events);
                }
            }
        }

        DrainTasks();
    }

    void Stop()
    {
        m_stopping = true;
        Wake();
    }

  private:
    struct IoOperation
    {
        std::uint8_t* m_readBytes{};
        const std::uint8_t* m_writeBytes{};
        std::size_t m_size{};
        std::size_t m_offset{};
        IoHandler m_handler;
    };

    struct SocketState
    {
        ConnectionId m_id{};
        int m_socket{-1};
        bool m_connecting{};
        bool m_closed{};
        ConnectHandler m_onConnect;
        IoOperation m_read;
        IoOperation m_write;
    };

    struct Timer
    {
        std::chrono::steady_clock::time_point m_due;
        Task m_task;
    };

    std::shared_ptr<SocketState> Find(ConnectionId connection) const
    {
        const auto found = m_connections.find(connection);
        return found == m_connections.end() ? nullptr : found->second;
    }

    void AddSocket(const std::shared_ptr<SocketState>& state)
    {
        epoll_event event{};
        event.events = EPOLLIN | EPOLLRDHUP |
                       (state->m_connecting ? static_cast<std::uint32_t>(EPOLLOUT) : 0U);
        event.data.fd = state->m_socket;
        if (epoll_ctl(m_epoll, EPOLL_CTL_ADD, state->m_socket, &event) == -1) {
            const std::string error = system_error(errno);
            Close(state->m_id);
            throw std::runtime_error(error);
        }
    }

    void UpdateEvents(const std::shared_ptr<SocketState>& state)
    {
        if (state->m_closed) {
            return;
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLRDHUP |
                       ((state->m_connecting || state->m_write.m_handler)
                            ? static_cast<std::uint32_t>(EPOLLOUT)
                            : 0U);
        event.data.fd = state->m_socket;
        if (epoll_ctl(m_epoll, EPOLL_CTL_MOD, state->m_socket, &event) == -1) {
            Close(state->m_id);
        }
    }

    void AcceptConnections()
    {
        while (true) {
            const int socketHandle =
                accept4(m_listener, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (socketHandle == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                return;
            }

            constexpr int ENABLED = 1;
            setsockopt(socketHandle, IPPROTO_TCP, TCP_NODELAY, &ENABLED, sizeof(ENABLED));

            auto state = std::make_shared<SocketState>();
            state->m_id = m_nextConnection++;
            state->m_socket = socketHandle;
            m_connections.emplace(state->m_id, state);
            m_socketConnections.emplace(socketHandle, state->m_id);
            try {
                AddSocket(state);
                m_onAccept(state->m_id);
            } catch (...) {
                Close(state->m_id);
            }
        }
    }

    void HandleSocketEvent(int socketHandle, std::uint32_t events)
    {
        const auto mapped = m_socketConnections.find(socketHandle);
        if (mapped == m_socketConnections.end()) {
            return;
        }
        auto state = Find(mapped->second);
        if (!state) {
            return;
        }

        if (state->m_connecting && (events & (EPOLLOUT | EPOLLERR | EPOLLHUP))) {
            int error = 0;
            socklen_t errorSize = sizeof(error);
            if (getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, &error, &errorSize) == -1) {
                error = errno;
            }
            CompleteConnect(state, error == 0 ? std::string{} : system_error(error));
        }
        if (state->m_closed) {
            return;
        }

        if (events & EPOLLIN) {
            ServiceRead(state);
        }
        if (!state->m_closed && events & EPOLLOUT) {
            ServiceWrite(state);
        }
        if (!state->m_closed && events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            ServiceRead(state);
        }
    }

    void ServiceRead(const std::shared_ptr<SocketState>& state)
    {
        auto& operation = state->m_read;
        while (!state->m_closed && operation.m_handler) {
            const auto remaining = operation.m_size - operation.m_offset;
            const ssize_t received =
                recv(state->m_socket, operation.m_readBytes + operation.m_offset, remaining, 0);
            if (received > 0) {
                operation.m_offset += static_cast<std::size_t>(received);
                if (operation.m_offset == operation.m_size) {
                    CompleteIo(operation, {});
                }
                continue;
            }
            if (received == 0) {
                CompleteIo(operation, "Connection closed by peer");
                return;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }

            CompleteIo(operation, system_error(errno));
            return;
        }
    }

    void ServiceWrite(const std::shared_ptr<SocketState>& state)
    {
        auto& operation = state->m_write;
        while (!state->m_closed && operation.m_handler) {
            const auto remaining = operation.m_size - operation.m_offset;
            const ssize_t sent = send(state->m_socket, operation.m_writeBytes + operation.m_offset,
                                      remaining, MSG_NOSIGNAL);
            if (sent > 0) {
                operation.m_offset += static_cast<std::size_t>(sent);
                if (operation.m_offset == operation.m_size) {
                    CompleteIo(operation, {});
                    UpdateEvents(state);
                }
                continue;
            }
            if (sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                UpdateEvents(state);
                return;
            }
            if (sent == -1 && errno == EINTR) {
                continue;
            }

            CompleteIo(operation, system_error(errno));
            UpdateEvents(state);
            return;
        }
    }

    void CompleteConnect(const std::shared_ptr<SocketState>& state, std::string error)
    {
        state->m_connecting = false;
        auto handler = std::move(state->m_onConnect);
        state->m_onConnect = {};
        if (error.empty()) {
            UpdateEvents(state);
        } else {
            state->m_closed = true;
            epoll_ctl(m_epoll, EPOLL_CTL_DEL, state->m_socket, nullptr);
            close(state->m_socket);
            m_socketConnections.erase(state->m_socket);
            m_connections.erase(state->m_id);
        }
        Queue([handler = std::move(handler), connection = state->m_id,
               error = std::move(error)]() mutable {
            handler(error.empty() ? connection : 0, std::move(error));
        });
    }

    void CompleteIo(IoOperation& operation, std::string error)
    {
        if (!operation.m_handler) {
            return;
        }

        auto handler = std::move(operation.m_handler);
        operation = {};
        Queue([handler = std::move(handler), error = std::move(error)]() mutable {
            handler(std::move(error));
        });
    }

    void Queue(Task task)
    {
        {
            std::lock_guard lock(m_taskMutex);
            m_tasks.push_back(std::move(task));
        }
        Wake();
    }

    void Wake()
    {
        constexpr std::uint64_t SIGNAL = 1;
        const auto result = write(m_wake, &SIGNAL, sizeof(SIGNAL));
        (void)result;
    }

    void DrainWake()
    {
        std::uint64_t signal = 0;
        while (read(m_wake, &signal, sizeof(signal)) == sizeof(signal)) {
        }
    }

    void DrainTasks()
    {
        std::deque<Task> tasks;
        {
            std::lock_guard lock(m_taskMutex);
            tasks.swap(m_tasks);
        }
        for (auto& task : tasks) {
            task();
        }
    }

    void RunTimers()
    {
        const auto now = std::chrono::steady_clock::now();
        std::deque<Task> ready;
        for (auto timer = m_timers.begin(); timer != m_timers.end();) {
            if (timer->second.m_due <= now) {
                ready.push_back(std::move(timer->second.m_task));
                timer = m_timers.erase(timer);
            } else {
                ++timer;
            }
        }
        for (auto& task : ready) {
            task();
        }
    }

    int WaitMilliseconds() const
    {
        if (m_timers.empty()) {
            return -1;
        }

        const auto earliest = std::min_element(m_timers.begin(), m_timers.end(),
                                               [](const auto& left, const auto& right) {
                                                   return left.second.m_due < right.second.m_due;
                                               });
        const auto remaining = earliest->second.m_due - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return 0;
        }

        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count() + 1;
        return static_cast<int>(
            std::min<std::int64_t>(milliseconds, std::numeric_limits<int>::max()));
    }

    int m_epoll{-1};
    int m_wake{-1};
    int m_listener{-1};
    AcceptHandler m_onAccept;

    std::unordered_map<ConnectionId, std::shared_ptr<SocketState>> m_connections;
    std::unordered_map<int, ConnectionId> m_socketConnections;
    ConnectionId m_nextConnection{1};

    std::map<TimerId, Timer> m_timers;
    TimerId m_nextTimer{1};

    std::mutex m_taskMutex;
    std::deque<Task> m_tasks;
    std::atomic<bool> m_stopping{false};
};

NativeEventLoop::NativeEventLoop() : m_impl(std::make_unique<Impl>()) {}

NativeEventLoop::~NativeEventLoop() = default;

std::uint16_t NativeEventLoop::Listen(const std::string& address, std::uint16_t port,
                                      AcceptHandler onAccept)
{
    return m_impl->Listen(address, port, std::move(onAccept));
}

void NativeEventLoop::Connect(const std::string& host, std::uint16_t port, ConnectHandler onConnect)
{
    m_impl->Connect(host, port, std::move(onConnect));
}

void NativeEventLoop::Read(ConnectionId connection, std::span<std::uint8_t> bytes,
                           IoHandler onComplete)
{
    m_impl->Read(connection, bytes, std::move(onComplete));
}

void NativeEventLoop::Write(ConnectionId connection, std::span<const std::uint8_t> bytes,
                            IoHandler onComplete)
{
    m_impl->Write(connection, bytes, std::move(onComplete));
}

void NativeEventLoop::Close(ConnectionId connection)
{
    m_impl->Close(connection);
}

bool NativeEventLoop::Open(ConnectionId connection) const
{
    return m_impl->Open(connection);
}

void NativeEventLoop::Post(Task task)
{
    m_impl->Post(std::move(task));
}

NativeEventLoop::TimerId NativeEventLoop::Schedule(std::chrono::steady_clock::duration delay,
                                                   Task task)
{
    return m_impl->Schedule(delay, std::move(task));
}

void NativeEventLoop::Cancel(TimerId timer)
{
    m_impl->Cancel(timer);
}

void NativeEventLoop::Run()
{
    m_impl->Run();
}

void NativeEventLoop::Stop()
{
    m_impl->Stop();
}

} // namespace MoongTangE
