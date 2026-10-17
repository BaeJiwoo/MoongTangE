#pragma once
#include "Common/Protocol/Wire.h"
#include <array>
#include <asio.hpp>
#include <deque>
#include <functional>
#include <memory>

namespace sandbox {
// All operations and callbacks run on the owning io_context's single thread.
class TcpChannel : public std::enable_shared_from_this<TcpChannel> {
public:
    using Socket = asio::ip::tcp::socket;

    explicit TcpChannel(Socket socket)
        : socket_(std::move(socket)), deadline_(socket_.get_executor()) {}

    std::function<void(const wire::Bytes&)> onMessage;
    std::function<void(std::string)> onClose;

    void start() {
        asio::error_code error;
        socket_.set_option(asio::ip::tcp::no_delay(true), error);
        readHeader();
    }

    void send(wire::Bytes payload) {
        if (closed_) {
            return;
        }
        if (out_.size() >= 32) {
            close("Slow peer: send queue full");
            return;
        }
        auto bytes = std::make_shared<wire::Bytes>(wire::frame(payload));
        const bool idle = out_.empty();
        out_.push_back(bytes);
        if (idle) {
            write();
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
        out_.clear();
        if (onClose) {
            onClose(std::move(reason));
        }
    }

private:
    void readHeader() {
        if (closed_) {
            return;
        }
        auto self = shared_from_this();
        deadline_.expires_after(std::chrono::seconds(10));
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
                             } catch (const std::exception& e) {
                                 self->close(e.what());
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
                if (!self->closed_ && self->onMessage) {
                    self->onMessage(self->body_);
                }
            } catch (const std::exception& e) {
                self->close(e.what());
            }
            self->readHeader();
        });
    }

    void write() {
        auto self = shared_from_this();
        auto bytes = out_.front();
        asio::async_write(socket_, asio::buffer(*bytes),
                          [self, bytes](asio::error_code error, std::size_t) {
                              if (error) {
                                  self->close(error.message());
                                  return;
                              }
                              if (self->closed_) {
                                  return;
                              }
                              self->out_.pop_front();
                              if (!self->out_.empty()) {
                                  self->write();
                              }
                          });
    }

    Socket socket_;
    asio::steady_timer deadline_;
    std::array<std::uint8_t, 4> header_{};
    wire::Bytes body_;
    std::deque<std::shared_ptr<wire::Bytes>> out_;
    bool closed_{};
};
} // namespace sandbox
