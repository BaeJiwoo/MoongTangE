#pragma once
#include "Network/Protocol/Codec.h"
#include <array>
#include <asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace sandbox {
// All operations/callbacks run on one io_context thread. Async work retains shared ownership.
class TcpChannel : public std::enable_shared_from_this<TcpChannel> {
public:
    using Socket = asio::ip::tcp::socket;
    using MessageHandler = std::function<void(const wire::Bytes&)>;
    using CloseHandler = std::function<void(std::string)>;

    explicit TcpChannel(Socket socket);
    void start(MessageHandler onMessage, CloseHandler onClose);
    void send(const wire::Bytes& payload);
    void close(std::string reason = "Connection closed");

private:
    static constexpr std::size_t MAX_QUEUED_FRAMES = 32;
    static constexpr auto READ_TIMEOUT = std::chrono::seconds(10);
    void readFrameHeader();
    void readFrameBody();
    void writeNextFrame();

    Socket m_socket;
    asio::steady_timer m_readDeadline;
    std::array<std::uint8_t, wire::FRAME_HEADER_BYTES> m_frameHeader{};
    wire::Bytes m_frameBody;
    std::deque<std::shared_ptr<wire::Bytes>> m_outboundFrames;
    MessageHandler m_onMessage;
    CloseHandler m_onClose;
    bool m_started{};
    bool m_closed{};
};
} // namespace sandbox
