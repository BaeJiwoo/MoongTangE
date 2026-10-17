#pragma once
#include "Network/Protocol/Codec.h"
#include <array>
#include <asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace sandbox {
// All operations/callbacks run on one io_context thread. Async work retains shared ownership.
class TcpChannel : public std::enable_shared_from_this<TcpChannel> {
public:
    using Socket = asio::ip::tcp::socket;
    using MessageHandler = std::function<void(const wire::Bytes&)>;
    using CloseHandler = std::function<void(std::string)>;

    explicit TcpChannel(Socket socket)
        : socket_(std::move(socket)), deadline_(socket_.get_executor()) {}

    void start(MessageHandler onMessage, CloseHandler onClose) {
        if (started_ || closed_ || weak_from_this().expired() || !onMessage || !onClose) {
            throw std::logic_error("A channel starts once with both handlers installed");
        }
        onMessage_ = std::move(onMessage);
        onClose_ = std::move(onClose);
        started_ = true;
        asio::error_code error;
        socket_.set_option(asio::ip::tcp::no_delay(true), error);
        readHeader();
    }

    void send(const wire::Bytes& payload) {
        if (closed_) {
            return;
        }
        if (outbound_.size() >= maximumQueuedFrames) {
            close("Slow peer: send queue full");
            return;
        }
        auto bytes = std::make_shared<wire::Bytes>(wire::frame(payload));
        const bool idle = outbound_.empty();
        outbound_.push_back(std::move(bytes));
        if (idle) {
            writeNext();
        }
    }

    void close(std::string reason = "Connection closed") {
        if (closed_) {
            return;
        }
        closed_ = true;
        asio::error_code ignored;
        deadline_.cancel();
        socket_.cancel(ignored);
        socket_.shutdown(Socket::shutdown_both, ignored);
        socket_.close(ignored);
        outbound_.clear();
        if (onClose_) {
            onClose_(std::move(reason));
        }
    }

private:
    static constexpr std::size_t maximumQueuedFrames = 32;
    static constexpr auto readTimeout = std::chrono::seconds(10);

    void readHeader() {
        if (closed_) {
            return;
        }
        auto self = shared_from_this();
        deadline_.expires_after(readTimeout);
        deadline_.async_wait([self](asio::error_code error) {
            if (!error) {
                self->close("Read timeout");
            }
        });
        asio::async_read(socket_, asio::buffer(header_),
                         [self](asio::error_code error, std::size_t) {
                             if (error) {
                                 self->close(error.message());
                                 return;
                             }
                             try {
                                 self->body_.resize(wire::frameSize(self->header_));
                                 self->readBody();
                             } catch (const std::exception& failure) {
                                 self->close(failure.what());
                             }
                         });
    }

    void readBody() {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(body_), [self](asio::error_code error, std::size_t) {
            if (error) {
                self->close(error.message());
                return;
            }
            try {
                if (!self->closed_) {
                    self->onMessage_(self->body_);
                }
            } catch (const std::exception& failure) {
                self->close(failure.what());
            }
            self->readHeader();
        });
    }

    void writeNext() {
        auto self = shared_from_this();
        // Retain the buffer even when close() clears the queue before completion.
        auto bytes = outbound_.front();
        asio::async_write(socket_, asio::buffer(*bytes),
                          [self, bytes](asio::error_code error, std::size_t) {
                              if (error) {
                                  self->close(error.message());
                                  return;
                              }
                              if (self->closed_) {
                                  return;
                              }
                              self->outbound_.pop_front();
                              if (!self->outbound_.empty()) {
                                  self->writeNext();
                              }
                          });
    }

    Socket socket_;
    asio::steady_timer deadline_;
    std::array<std::uint8_t, wire::frameHeaderBytes> header_{};
    wire::Bytes body_;
    std::deque<std::shared_ptr<wire::Bytes>> outbound_;
    MessageHandler onMessage_;
    CloseHandler onClose_;
    bool started_{};
    bool closed_{};
};
} // namespace sandbox
