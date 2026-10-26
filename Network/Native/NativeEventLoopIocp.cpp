#include "Network/Native/NativeEventLoop.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <limits>
#include <map>
#include <mswsock.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace MoongTangE {
namespace {

constexpr ULONG_PTR WAKE_COMPLETION_KEY = 1;

std::string windows_error(DWORD error)
{
    char* message = nullptr;
    const DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<char*>(&message), 0, nullptr);
    std::string result =
        size == 0 ? "Windows socket error " + std::to_string(error) : std::string(message, size);
    if (message) {
        LocalFree(message);
    }
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) {
        result.pop_back();
    }
    return result;
}

} // namespace

class NativeEventLoop::Impl
{
  public:
    Impl()
    {
        WSADATA data{};
        const int startupError = WSAStartup(MAKEWORD(2, 2), &data);
        if (startupError != 0) {
            throw std::runtime_error(windows_error(static_cast<DWORD>(startupError)));
        }

        m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
        if (!m_completionPort) {
            const DWORD error = GetLastError();
            WSACleanup();
            throw std::runtime_error(windows_error(error));
        }
    }

    ~Impl()
    {
        for (const auto& [connection, state] : m_connections) {
            (void)connection;
            CancelIoEx(reinterpret_cast<HANDLE>(state->m_socket), nullptr);
            closesocket(state->m_socket);
        }
        if (m_listener != INVALID_SOCKET) {
            CancelIoEx(reinterpret_cast<HANDLE>(m_listener), nullptr);
            closesocket(m_listener);
        }

        CloseHandle(m_completionPort);
        for (auto* operation : m_operations) {
            if (operation->m_acceptSocket != INVALID_SOCKET) {
                closesocket(operation->m_acceptSocket);
            }
            delete operation;
        }
        WSACleanup();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::uint16_t Listen(const std::string& address, std::uint16_t port, AcceptHandler onAccept)
    {
        if (m_listener != INVALID_SOCKET || !onAccept) {
            throw std::logic_error("The native event loop owns one listener");
        }

        SOCKET listener =
            WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (listener == INVALID_SOCKET) {
            throw std::runtime_error(windows_error(WSAGetLastError()));
        }

        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = htons(port);
        if (InetPtonA(AF_INET, address.c_str(), &endpoint.sin_addr) != 1 ||
            bind(listener, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) ==
                SOCKET_ERROR ||
            listen(listener, SOMAXCONN) == SOCKET_ERROR) {
            const DWORD error = WSAGetLastError();
            closesocket(listener);
            throw std::runtime_error(windows_error(error));
        }

        if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(listener), m_completionPort, 0, 0)) {
            const DWORD error = GetLastError();
            closesocket(listener);
            throw std::runtime_error(windows_error(error));
        }

        GUID acceptGuid = WSAID_ACCEPTEX;
        DWORD bytes = 0;
        LPFN_ACCEPTEX acceptFunction = nullptr;
        if (WSAIoctl(listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptGuid, sizeof(acceptGuid),
                     &acceptFunction, sizeof(acceptFunction), &bytes, nullptr,
                     nullptr) == SOCKET_ERROR) {
            const DWORD error = WSAGetLastError();
            closesocket(listener);
            throw std::runtime_error(windows_error(error));
        }

        sockaddr_in boundAddress{};
        int boundSize = sizeof(boundAddress);
        if (getsockname(listener, reinterpret_cast<sockaddr*>(&boundAddress), &boundSize) ==
            SOCKET_ERROR) {
            const DWORD error = WSAGetLastError();
            closesocket(listener);
            throw std::runtime_error(windows_error(error));
        }

        m_listener = listener;
        m_acceptFunction = acceptFunction;
        m_onAccept = std::move(onAccept);
        SubmitAccept();
        return ntohs(boundAddress.sin_port);
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
        if (resolveError != 0 || !addresses) {
            Post(
                [handler = std::move(onConnect), error = std::string(gai_strerrorA(resolveError))] {
                    handler(0, error);
                });
            return;
        }

        SOCKET socketHandle =
            WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (socketHandle == INVALID_SOCKET) {
            const DWORD error = WSAGetLastError();
            freeaddrinfo(addresses);
            Post([handler = std::move(onConnect), error] {
                handler(0, windows_error(error));
            });
            return;
        }

        sockaddr_in localAddress{};
        localAddress.sin_family = AF_INET;
        localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&localAddress),
                 sizeof(localAddress)) == SOCKET_ERROR ||
            !CreateIoCompletionPort(reinterpret_cast<HANDLE>(socketHandle), m_completionPort, 0,
                                    0)) {
            const DWORD error = WSAGetLastError();
            closesocket(socketHandle);
            freeaddrinfo(addresses);
            Post([handler = std::move(onConnect), error] {
                handler(0, windows_error(error));
            });
            return;
        }

        GUID connectGuid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        LPFN_CONNECTEX connectFunction = nullptr;
        if (WSAIoctl(socketHandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &connectGuid,
                     sizeof(connectGuid), &connectFunction, sizeof(connectFunction), &bytes,
                     nullptr, nullptr) == SOCKET_ERROR) {
            const DWORD error = WSAGetLastError();
            closesocket(socketHandle);
            freeaddrinfo(addresses);
            Post([handler = std::move(onConnect), error] {
                handler(0, windows_error(error));
            });
            return;
        }

        auto state = std::make_shared<SocketState>();
        state->m_id = m_nextConnection++;
        state->m_socket = socketHandle;
        state->m_onConnect = std::move(onConnect);
        m_connections.emplace(state->m_id, state);

        auto* operation = NewOperation(OperationKind::CONNECT, state);
        state->m_connect = operation;
        const BOOL started = connectFunction(socketHandle, addresses->ai_addr,
                                             static_cast<int>(addresses->ai_addrlen), nullptr, 0,
                                             nullptr, &operation->m_overlapped);
        const DWORD error = started ? ERROR_SUCCESS : WSAGetLastError();
        freeaddrinfo(addresses);
        if (!started && error != ERROR_IO_PENDING) {
            CompleteConnect(operation, error);
        }
    }

    void Read(ConnectionId connection, std::span<std::uint8_t> bytes, IoHandler onComplete)
    {
        auto state = Find(connection);
        if (!state || state->m_read || bytes.empty()) {
            Post([handler = std::move(onComplete)] {
                handler("Invalid native read operation");
            });
            return;
        }

        auto* operation = NewOperation(OperationKind::READ, state);
        operation->m_readBytes = bytes.data();
        operation->m_size = bytes.size();
        operation->m_handler = std::move(onComplete);
        state->m_read = operation;
        IssueRead(operation);
    }

    void Write(ConnectionId connection, std::span<const std::uint8_t> bytes, IoHandler onComplete)
    {
        auto state = Find(connection);
        if (!state || state->m_write || bytes.empty()) {
            Post([handler = std::move(onComplete)] {
                handler("Invalid native write operation");
            });
            return;
        }

        auto* operation = NewOperation(OperationKind::WRITE, state);
        operation->m_writeBytes = bytes.data();
        operation->m_size = bytes.size();
        operation->m_handler = std::move(onComplete);
        state->m_write = operation;
        IssueWrite(operation);
    }

    void Close(ConnectionId connection)
    {
        auto state = Find(connection);
        if (!state || state->m_closed) {
            return;
        }

        state->m_closed = true;
        CancelIoEx(reinterpret_cast<HANDLE>(state->m_socket), nullptr);
        shutdown(state->m_socket, SD_BOTH);
        closesocket(state->m_socket);
        state->m_socket = INVALID_SOCKET;
        m_connections.erase(connection);
    }

    bool Open(ConnectionId connection) const
    {
        const auto found = m_connections.find(connection);
        return found != m_connections.end() && !found->second->m_closed;
    }

    void Post(Task task)
    {
        {
            std::lock_guard lock(m_taskMutex);
            m_tasks.push_back(std::move(task));
        }
        PostQueuedCompletionStatus(m_completionPort, 0, WAKE_COMPLETION_KEY, nullptr);
    }

    TimerId Schedule(std::chrono::steady_clock::duration delay, Task task)
    {
        const TimerId timer = m_nextTimer++;
        m_timers.emplace(timer, Timer{std::chrono::steady_clock::now() + delay, std::move(task)});
        PostQueuedCompletionStatus(m_completionPort, 0, WAKE_COMPLETION_KEY, nullptr);
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
        while (!m_stopping) {
            DrainTasks();
            RunTimers();
            if (m_stopping) {
                break;
            }

            DWORD transferred = 0;
            ULONG_PTR key = 0;
            OVERLAPPED* overlapped = nullptr;
            const BOOL completed = GetQueuedCompletionStatus(m_completionPort, &transferred, &key,
                                                             &overlapped, WaitMilliseconds());
            if (!overlapped) {
                if (!completed && GetLastError() != WAIT_TIMEOUT) {
                    throw std::runtime_error(windows_error(GetLastError()));
                }
                continue;
            }

            auto* operation = reinterpret_cast<Operation*>(overlapped);
            const DWORD error = completed ? ERROR_SUCCESS : GetLastError();
            HandleCompletion(operation, transferred, error);
        }

        DrainTasks();
    }

    void Stop()
    {
        m_stopping = true;
        PostQueuedCompletionStatus(m_completionPort, 0, WAKE_COMPLETION_KEY, nullptr);
    }

  private:
    enum class OperationKind
    {
        ACCEPT,
        CONNECT,
        READ,
        WRITE
    };

    struct Operation;

    struct SocketState
    {
        ConnectionId m_id{};
        SOCKET m_socket{INVALID_SOCKET};
        bool m_closed{};
        ConnectHandler m_onConnect;
        Operation* m_connect{};
        Operation* m_read{};
        Operation* m_write{};
    };

    struct Operation
    {
        OVERLAPPED m_overlapped{};
        OperationKind m_kind{OperationKind::READ};
        std::shared_ptr<SocketState> m_state;
        SOCKET m_acceptSocket{INVALID_SOCKET};
        std::array<char, (sizeof(sockaddr_in) + 16) * 2> m_acceptAddresses{};
        std::uint8_t* m_readBytes{};
        const std::uint8_t* m_writeBytes{};
        std::size_t m_size{};
        std::size_t m_offset{};
        IoHandler m_handler;
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

    Operation* NewOperation(OperationKind kind, std::shared_ptr<SocketState> state = {})
    {
        auto* operation = new Operation{};
        operation->m_kind = kind;
        operation->m_state = std::move(state);
        m_operations.insert(operation);
        return operation;
    }

    void DeleteOperation(Operation* operation)
    {
        m_operations.erase(operation);
        delete operation;
    }

    void SubmitAccept()
    {
        if (m_stopping || m_listener == INVALID_SOCKET) {
            return;
        }

        auto* operation = NewOperation(OperationKind::ACCEPT);
        operation->m_acceptSocket =
            WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (operation->m_acceptSocket == INVALID_SOCKET) {
            DeleteOperation(operation);
            return;
        }

        DWORD received = 0;
        const BOOL started =
            m_acceptFunction(m_listener, operation->m_acceptSocket,
                             operation->m_acceptAddresses.data(), 0, sizeof(sockaddr_in) + 16,
                             sizeof(sockaddr_in) + 16, &received, &operation->m_overlapped);
        const DWORD error = started ? ERROR_SUCCESS : WSAGetLastError();
        if (!started && error != ERROR_IO_PENDING) {
            closesocket(operation->m_acceptSocket);
            operation->m_acceptSocket = INVALID_SOCKET;
            DeleteOperation(operation);
            Post([this] {
                SubmitAccept();
            });
        }
    }

    void HandleCompletion(Operation* operation, DWORD transferred, DWORD error)
    {
        switch (operation->m_kind) {
        case OperationKind::ACCEPT:
            CompleteAccept(operation, error);
            break;
        case OperationKind::CONNECT:
            CompleteConnect(operation, error);
            break;
        case OperationKind::READ:
            CompleteRead(operation, transferred, error);
            break;
        case OperationKind::WRITE:
            CompleteWrite(operation, transferred, error);
            break;
        }
    }

    void CompleteAccept(Operation* operation, DWORD error)
    {
        const SOCKET accepted = operation->m_acceptSocket;
        operation->m_acceptSocket = INVALID_SOCKET;
        DeleteOperation(operation);

        if (error != ERROR_SUCCESS || m_stopping) {
            closesocket(accepted);
            SubmitAccept();
            return;
        }

        setsockopt(accepted, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                   reinterpret_cast<const char*>(&m_listener), sizeof(m_listener));
        constexpr BOOL ENABLED = TRUE;
        setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&ENABLED),
                   sizeof(ENABLED));
        if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(accepted), m_completionPort, 0, 0)) {
            closesocket(accepted);
            SubmitAccept();
            return;
        }

        auto state = std::make_shared<SocketState>();
        state->m_id = m_nextConnection++;
        state->m_socket = accepted;
        m_connections.emplace(state->m_id, state);
        m_onAccept(state->m_id);
        SubmitAccept();
    }

    void CompleteConnect(Operation* operation, DWORD error)
    {
        auto state = operation->m_state;
        state->m_connect = nullptr;
        auto handler = std::move(state->m_onConnect);
        DeleteOperation(operation);

        if (error == ERROR_SUCCESS && !state->m_closed) {
            setsockopt(state->m_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0);
            handler(state->m_id, {});
            return;
        }

        const DWORD connectionError = error == ERROR_SUCCESS ? ERROR_OPERATION_ABORTED : error;
        state->m_closed = true;
        if (state->m_socket != INVALID_SOCKET) {
            closesocket(state->m_socket);
            state->m_socket = INVALID_SOCKET;
        }
        m_connections.erase(state->m_id);
        handler(0, windows_error(connectionError));
    }

    void CompleteRead(Operation* operation, DWORD transferred, DWORD error)
    {
        auto state = operation->m_state;
        if (error == ERROR_SUCCESS && transferred != 0) {
            operation->m_offset += transferred;
            if (operation->m_offset < operation->m_size) {
                IssueRead(operation);
                return;
            }
        }

        state->m_read = nullptr;
        auto handler = std::move(operation->m_handler);
        DeleteOperation(operation);
        if (error != ERROR_SUCCESS) {
            handler(windows_error(error));
        } else if (transferred == 0) {
            handler("Connection closed by peer");
        } else {
            handler({});
        }
    }

    void CompleteWrite(Operation* operation, DWORD transferred, DWORD error)
    {
        auto state = operation->m_state;
        if (error == ERROR_SUCCESS && transferred != 0) {
            operation->m_offset += transferred;
            if (operation->m_offset < operation->m_size) {
                IssueWrite(operation);
                return;
            }
        }

        state->m_write = nullptr;
        auto handler = std::move(operation->m_handler);
        DeleteOperation(operation);
        if (error != ERROR_SUCCESS) {
            handler(windows_error(error));
        } else if (transferred == 0) {
            handler("Connection closed while writing");
        } else {
            handler({});
        }
    }

    void IssueRead(Operation* operation)
    {
        operation->m_overlapped = {};
        WSABUF buffer{};
        buffer.buf = reinterpret_cast<char*>(operation->m_readBytes + operation->m_offset);
        buffer.len = static_cast<ULONG>(operation->m_size - operation->m_offset);
        DWORD flags = 0;
        DWORD received = 0;
        const int result = WSARecv(operation->m_state->m_socket, &buffer, 1, &received, &flags,
                                   &operation->m_overlapped, nullptr);
        const DWORD error = result == 0 ? ERROR_SUCCESS : WSAGetLastError();
        if (result == SOCKET_ERROR && error != ERROR_IO_PENDING) {
            CompleteRead(operation, 0, error);
        }
    }

    void IssueWrite(Operation* operation)
    {
        operation->m_overlapped = {};
        WSABUF buffer{};
        buffer.buf = reinterpret_cast<char*>(
            const_cast<std::uint8_t*>(operation->m_writeBytes + operation->m_offset));
        buffer.len = static_cast<ULONG>(operation->m_size - operation->m_offset);
        DWORD sent = 0;
        const int result = WSASend(operation->m_state->m_socket, &buffer, 1, &sent, 0,
                                   &operation->m_overlapped, nullptr);
        const DWORD error = result == 0 ? ERROR_SUCCESS : WSAGetLastError();
        if (result == SOCKET_ERROR && error != ERROR_IO_PENDING) {
            CompleteWrite(operation, 0, error);
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

    DWORD WaitMilliseconds() const
    {
        if (m_timers.empty()) {
            return INFINITE;
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
        return static_cast<DWORD>(
            std::min<std::int64_t>(milliseconds, std::numeric_limits<DWORD>::max() - 1));
    }

    HANDLE m_completionPort{};
    SOCKET m_listener{INVALID_SOCKET};
    LPFN_ACCEPTEX m_acceptFunction{};
    AcceptHandler m_onAccept;

    std::unordered_map<ConnectionId, std::shared_ptr<SocketState>> m_connections;
    std::unordered_set<Operation*> m_operations;
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
