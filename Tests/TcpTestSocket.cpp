#include "TcpTestSocket.h"

#include <algorithm>
#include <array>
#include <climits>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace MoongTangE::test {
namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;

class SocketRuntime
{
  public:
    SocketRuntime()
    {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~SocketRuntime()
    {
        WSACleanup();
    }

    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;
    SocketRuntime(SocketRuntime&&) = delete;
    SocketRuntime& operator=(SocketRuntime&&) = delete;
};

void ensure_socket_runtime()
{
    static SocketRuntime runtime;
}

int last_socket_error()
{
    return WSAGetLastError();
}

void close_socket(SocketHandle socket)
{
    if (socket != INVALID_SOCKET_HANDLE) {
        closesocket(socket);
    }
}
#else
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;

void ensure_socket_runtime() {}

int last_socket_error()
{
    return errno;
}

void close_socket(SocketHandle socket)
{
    if (socket != INVALID_SOCKET_HANDLE) {
        close(socket);
    }
}
#endif

[[noreturn]] void throw_socket_error(std::string_view operation)
{
    throw std::runtime_error(std::string(operation) + " failed with socket error " +
                             std::to_string(last_socket_error()));
}

sockaddr_in loopback_endpoint(std::uint16_t port)
{
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port);
    endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return endpoint;
}

std::size_t socket_chunk(std::size_t remaining)
{
    return std::min(remaining, static_cast<std::size_t>(INT_MAX));
}

} // namespace

// Owns one connected OS socket and closes it when the test socket releases this state.
struct TcpTestSocket::ConnectedSocketState
{
    explicit ConnectedSocketState(SocketHandle socket) : m_socket(socket) {}

    ConnectedSocketState(const ConnectedSocketState&) = delete;
    ConnectedSocketState& operator=(const ConnectedSocketState&) = delete;
    ConnectedSocketState(ConnectedSocketState&&) = delete;
    ConnectedSocketState& operator=(ConnectedSocketState&&) = delete;

    ~ConnectedSocketState()
    {
        close_socket(m_socket);
    }

    SocketHandle m_socket = INVALID_SOCKET_HANDLE;
};

// Owns the listening OS socket and its bound port, hidden from the public test helper.
struct TcpTestListener::ListenerState
{
    ListenerState() = default;
    ListenerState(const ListenerState&) = delete;
    ListenerState& operator=(const ListenerState&) = delete;
    ListenerState(ListenerState&&) = delete;
    ListenerState& operator=(ListenerState&&) = delete;

    ~ListenerState()
    {
        close_socket(m_socket);
    }

    SocketHandle m_socket = INVALID_SOCKET_HANDLE;
    std::uint16_t m_port = 0;
};

TcpTestSocket::TcpTestSocket(std::unique_ptr<ConnectedSocketState> socketState)
    : m_socketState(std::move(socketState))
{
}

TcpTestSocket::~TcpTestSocket() = default;
TcpTestSocket::TcpTestSocket(TcpTestSocket&&) noexcept = default;
TcpTestSocket& TcpTestSocket::operator=(TcpTestSocket&&) noexcept = default;

TcpTestSocket TcpTestSocket::Connect(std::string_view address, std::uint16_t port)
{
    ensure_socket_runtime();
    if (address != "127.0.0.1") {
        throw std::invalid_argument("TcpTestSocket supports only the IPv4 loopback address");
    }

    const auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == INVALID_SOCKET_HANDLE) {
        throw_socket_error("socket");
    }

    const auto endpoint = loopback_endpoint(port);
    if (::connect(socket, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) != 0) {
        const auto error = last_socket_error();
        close_socket(socket);
        throw std::runtime_error("connect failed with socket error " + std::to_string(error));
    }
    // Allocate the implementation state and transfer ownership of the connected socket.
    return TcpTestSocket(std::make_unique<ConnectedSocketState>(socket));
}

void TcpTestSocket::ReadExact(std::span<std::uint8_t> destination)
{
    std::size_t offset = 0;
    while (offset < destination.size()) {
        const auto remaining = socket_chunk(destination.size() - offset);
#if defined(_WIN32)
        const auto received =
            recv(m_socketState->m_socket, reinterpret_cast<char*>(destination.data() + offset),
                 static_cast<int>(remaining), 0);
#else
        const auto received =
            recv(m_socketState->m_socket, destination.data() + offset, remaining, 0);
#endif
        if (received <= 0) {
            throw_socket_error("recv");
        }
        offset += static_cast<std::size_t>(received);
    }
}

void TcpTestSocket::Write(std::span<const std::uint8_t> source)
{
    std::size_t offset = 0;
    while (offset < source.size()) {
        const auto remaining = socket_chunk(source.size() - offset);
#if defined(_WIN32)
        const auto sent =
            send(m_socketState->m_socket, reinterpret_cast<const char*>(source.data() + offset),
                 static_cast<int>(remaining), 0);
#else
        const auto sent = send(m_socketState->m_socket, source.data() + offset, remaining, 0);
#endif
        if (sent <= 0) {
            throw_socket_error("send");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

void TcpTestSocket::WriteByte(std::uint8_t value)
{
    Write(std::span(&value, 1));
}

bool TcpTestSocket::Closed() const
{
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(m_socketState->m_socket, &readable);
    timeval timeout{};
#if defined(_WIN32)
    const auto ready = select(0, &readable, nullptr, nullptr, &timeout);
#else
    const auto ready = select(m_socketState->m_socket + 1, &readable, nullptr, nullptr, &timeout);
#endif
    if (ready < 0) {
        throw_socket_error("select");
    }
    if (ready == 0) {
        return false;
    }

    std::array<char, 1> data{};
    const auto received = recv(m_socketState->m_socket, data.data(), 1, MSG_PEEK);
    return received <= 0;
}

// Allocate the listener implementation state before opening its OS socket.
TcpTestListener::TcpTestListener() : m_listenerState(std::make_unique<ListenerState>())
{
    ensure_socket_runtime();
    m_listenerState->m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenerState->m_socket == INVALID_SOCKET_HANDLE) {
        throw_socket_error("socket");
    }

    const auto endpoint = loopback_endpoint(0);
    if (bind(m_listenerState->m_socket, reinterpret_cast<const sockaddr*>(&endpoint),
             sizeof(endpoint)) != 0) {
        throw_socket_error("bind");
    }
    if (listen(m_listenerState->m_socket, SOMAXCONN) != 0) {
        throw_socket_error("listen");
    }

    sockaddr_in bound{};
#if defined(_WIN32)
    int size = sizeof(bound);
#else
    socklen_t size = sizeof(bound);
#endif
    if (getsockname(m_listenerState->m_socket, reinterpret_cast<sockaddr*>(&bound), &size) != 0) {
        throw_socket_error("getsockname");
    }
    m_listenerState->m_port = ntohs(bound.sin_port);
}

TcpTestListener::~TcpTestListener() = default;
TcpTestListener::TcpTestListener(TcpTestListener&&) noexcept = default;
TcpTestListener& TcpTestListener::operator=(TcpTestListener&&) noexcept = default;

std::uint16_t TcpTestListener::Port() const
{
    return m_listenerState->m_port;
}

TcpTestSocket TcpTestListener::Accept() const
{
    const auto socket = accept(m_listenerState->m_socket, nullptr, nullptr);
    if (socket == INVALID_SOCKET_HANDLE) {
        throw_socket_error("accept");
    }
    // Allocate the implementation state and transfer ownership of the accepted socket.
    return TcpTestSocket(std::make_unique<TcpTestSocket::ConnectedSocketState>(socket));
}

void TcpTestListener::Close()
{
    close_socket(std::exchange(m_listenerState->m_socket, INVALID_SOCKET_HANDLE));
}

} // namespace MoongTangE::test
