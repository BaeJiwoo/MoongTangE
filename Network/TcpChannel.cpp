#include "Network/TcpChannel.h"
#include <exception>
#include <stdexcept>
#include <utility>

namespace sandbox {
TcpChannel::TcpChannel(Socket socket)
    : m_socket(std::move(socket)), m_readDeadline(m_socket.get_executor()) {}

void TcpChannel::start(MessageHandler onMessage, CloseHandler onClose) {
    if (m_started || m_closed || weak_from_this().expired() || !onMessage || !onClose) {
        throw std::logic_error("A channel starts once with both handlers installed");
    }
    m_onMessage = std::move(onMessage);
    m_onClose = std::move(onClose);
    m_started = true;
    asio::error_code error;
    m_socket.set_option(asio::ip::tcp::no_delay(true), error);
    readFrameHeader();
}

void TcpChannel::send(const wire::Bytes& payload) {
    if (m_closed) {
        return;
    }
    if (m_outboundFrames.size() >= MAX_QUEUED_FRAMES) {
        close("Slow peer: send queue full");
        return;
    }
    auto bytes = std::make_shared<wire::Bytes>(wire::frame(payload));
    const bool idle = m_outboundFrames.empty();
    m_outboundFrames.push_back(std::move(bytes));
    if (idle) {
        writeNextFrame();
    }
}

void TcpChannel::close(std::string reason) {
    if (m_closed) {
        return;
    }
    m_closed = true;
    asio::error_code ignored;
    m_readDeadline.cancel();
    m_socket.cancel(ignored);
    m_socket.shutdown(Socket::shutdown_both, ignored);
    m_socket.close(ignored);
    m_outboundFrames.clear();
    if (m_onClose) {
        m_onClose(std::move(reason));
    }
}

void TcpChannel::readFrameHeader() {
    if (m_closed) {
        return;
    }
    auto self = shared_from_this();
    m_readDeadline.expires_after(READ_TIMEOUT);
    m_readDeadline.async_wait([self](asio::error_code error) {
        if (!error) {
            self->close("Read timeout");
        }
    });
    asio::async_read(m_socket, asio::buffer(m_frameHeader),
                     [self](asio::error_code error, std::size_t) {
                         if (error) {
                             self->close(error.message());
                             return;
                         }
                         try {
                             self->m_frameBody.resize(wire::frameSize(self->m_frameHeader));
                             self->readFrameBody();
                         } catch (const std::exception& failure) {
                             self->close(failure.what());
                         }
                     });
}

void TcpChannel::readFrameBody() {
    auto self = shared_from_this();
    asio::async_read(m_socket, asio::buffer(m_frameBody),
                     [self](asio::error_code error, std::size_t) {
                         if (error) {
                             self->close(error.message());
                             return;
                         }
                         try {
                             if (!self->m_closed) {
                                 self->m_onMessage(self->m_frameBody);
                             }
                         } catch (const std::exception& failure) {
                             self->close(failure.what());
                         }
                         self->readFrameHeader();
                     });
}

void TcpChannel::writeNextFrame() {
    auto self = shared_from_this();
    // Retain the buffer even when close() clears the queue before completion.
    auto bytes = m_outboundFrames.front();
    asio::async_write(m_socket, asio::buffer(*bytes),
                      [self, bytes](asio::error_code error, std::size_t) {
                          if (error) {
                              self->close(error.message());
                              return;
                          }
                          if (self->m_closed) {
                              return;
                          }
                          self->m_outboundFrames.pop_front();
                          if (!self->m_outboundFrames.empty()) {
                              self->writeNextFrame();
                          }
                      });
}
} // namespace sandbox
