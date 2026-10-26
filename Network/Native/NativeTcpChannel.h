#pragma once

#include "Network/Native/NativeEventLoop.h"
#include "Network/Protocol/Codec.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace MoongTangE {

// All operations and callbacks run on one NativeEventLoop thread.
class NativeTcpChannel : public std::enable_shared_from_this<NativeTcpChannel>
{
  public:
    using MessageHandler = std::function<void(const wire::Bytes&)>;
    using CloseHandler = std::function<void(std::string)>;

    NativeTcpChannel(NativeEventLoop& eventLoop, NativeEventLoop::ConnectionId connection);

    void Start(MessageHandler onMessage, CloseHandler onClose);
    void Send(const wire::Bytes& payload);
    void SendAndClose(const wire::Bytes& payload, std::string reason);
    void Close(std::string reason = "Connection closed");

  private:
    static constexpr std::size_t MAX_QUEUED_FRAMES = 32;
    static constexpr auto READ_TIMEOUT = std::chrono::seconds(10);

    void ReadFrameHeader();
    void ReadFrameBody();
    void WriteNextFrame();

    NativeEventLoop& m_eventLoop;
    NativeEventLoop::ConnectionId m_connection;
    NativeEventLoop::TimerId m_readDeadline{};

    std::array<std::uint8_t, wire::FRAME_HEADER_BYTES> m_frameHeader{};
    wire::Bytes m_frameBody;
    std::deque<std::shared_ptr<wire::Bytes>> m_outboundFrames;

    MessageHandler m_onMessage;
    CloseHandler m_onClose;

    bool m_started{};
    bool m_closed{};
    bool m_draining{};
    std::string m_closeReason;
};

} // namespace MoongTangE
