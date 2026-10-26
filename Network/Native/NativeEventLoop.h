#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace MoongTangE {

// Platform event loop implemented by epoll on Linux and IOCP on Windows.
class NativeEventLoop
{
  public:
    using ConnectionId = std::uint64_t;
    using TimerId = std::uint64_t;
    using Task = std::function<void()>;
    using AcceptHandler = std::function<void(ConnectionId)>;
    using ConnectHandler = std::function<void(ConnectionId, std::string)>;
    using IoHandler = std::function<void(std::string)>;

    NativeEventLoop();
    ~NativeEventLoop();

    NativeEventLoop(const NativeEventLoop&) = delete;
    NativeEventLoop& operator=(const NativeEventLoop&) = delete;
    NativeEventLoop(NativeEventLoop&&) = delete;
    NativeEventLoop& operator=(NativeEventLoop&&) = delete;

    std::uint16_t Listen(const std::string& address, std::uint16_t port, AcceptHandler onAccept);
    void Connect(const std::string& host, std::uint16_t port, ConnectHandler onConnect);

    void Read(ConnectionId connection, std::span<std::uint8_t> bytes, IoHandler onComplete);
    void Write(ConnectionId connection, std::span<const std::uint8_t> bytes, IoHandler onComplete);
    void Close(ConnectionId connection);
    bool Open(ConnectionId connection) const;

    void Post(Task task);
    TimerId Schedule(std::chrono::steady_clock::duration delay, Task task);
    void Cancel(TimerId timer);

    void Run();
    void Stop();

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace MoongTangE
