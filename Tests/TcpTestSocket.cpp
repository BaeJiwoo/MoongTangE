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

struct TcpTestSocket::Impl
{
    explicit Impl(SocketHandle socket) : m_socket(socket) {}

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl()
    {
        close_socket(m_socket);
    }

    SocketHandle m_socket = INVALID_SOCKET_HANDLE;
};

struct TcpTestListener::Impl
{
    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl()
    {
        close_socket(m_socket);
    }

    SocketHandle m_socket = INVALID_SOCKET_HANDLE;
    std::uint16_t m_port = 0;
};

TcpTestSocket::TcpTestSocket(std::unique_ptr<Impl> implementation)
    : m_impl(std::move(implementation))
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
    return TcpTestSocket(std::make_unique<Impl>(socket));
}

void TcpTestSocket::ReadExact(std::span<std::uint8_t> destination)
{
    std::size_t offset = 0;
    while (offset < destination.size()) {
        const auto remaining = socket_chunk(destination.size() - offset);
#if defined(_WIN32)
        const auto received =
            recv(m_impl->m_socket, reinterpret_cast<char*>(destination.data() + offset),
                 static_cast<int>(remaining), 0);
#else
        const auto received = recv(m_impl->m_socket, destination.data() + offset, remaining, 0);
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
            send(m_impl->m_socket, reinterpret_cast<const char*>(source.data() + offset),
                 static_cast<int>(remaining), 0);
#else
        const auto sent = send(m_impl->m_socket, source.data() + offset, remaining, 0);
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
    FD_SET(m_impl->m_socket, &readable);
    timeval timeout{};
#if defined(_WIN32)
    const auto ready = select(0, &readable, nullptr, nullptr, &timeout);
#else
    const auto ready = select(m_impl->m_socket + 1, &readable, nullptr, nullptr, &timeout);
#endif
    if (ready < 0) {
        throw_socket_error("select");
    }
    if (ready == 0) {
        return false;
    }

    std::array<char, 1> data{};
    const auto received = recv(m_impl->m_socket, data.data(), 1, MSG_PEEK);
    return received <= 0;
}

TcpTestListener::TcpTestListener() : m_impl(std::make_unique<Impl>())
{
    ensure_socket_runtime();
    m_impl->m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_impl->m_socket == INVALID_SOCKET_HANDLE) {
        throw_socket_error("socket");
    }

    const auto endpoint = loopback_endpoint(0);
    if (bind(m_impl->m_socket, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) !=
        0) {
        throw_socket_error("bind");
    }
    if (listen(m_impl->m_socket, SOMAXCONN) != 0) {
        throw_socket_error("listen");
    }

    sockaddr_in bound{};
#if defined(_WIN32)
    int size = sizeof(bound);
#else
    socklen_t size = sizeof(bound);
#endif
    if (getsockname(m_impl->m_socket, reinterpret_cast<sockaddr*>(&bound), &size) != 0) {
        throw_socket_error("getsockname");
    }
    m_impl->m_port = ntohs(bound.sin_port);
}

TcpTestListener::~TcpTestListener() = default;
TcpTestListener::TcpTestListener(TcpTestListener&&) noexcept = default;
TcpTestListener& TcpTestListener::operator=(TcpTestListener&&) noexcept = default;

std::uint16_t TcpTestListener::Port() const
{
    return m_impl->m_port;
}

TcpTestSocket TcpTestListener::Accept() const
{
    const auto socket = accept(m_impl->m_socket, nullptr, nullptr);
    if (socket == INVALID_SOCKET_HANDLE) {
        throw_socket_error("accept");
    }
    return TcpTestSocket(std::make_unique<TcpTestSocket::Impl>(socket));
}

void TcpTestListener::Close()
{
    close_socket(std::exchange(m_impl->m_socket, INVALID_SOCKET_HANDLE));
}

} // namespace MoongTangE::test
