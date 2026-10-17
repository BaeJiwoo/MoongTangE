#include "Client/Transport/TcpClientTransport.h"
#include "Network/Protocol/Codec.h"
#include "Network/TcpChannel.h"
#include <asio.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace sandbox {
class TcpClientTransport::Impl {
public:
    Impl(const std::string& host, std::uint16_t port) {
        beginConnection(host, port);
        thread_ = std::thread([this] {
            io_.run();
        });
    }

    ~Impl() {
        stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    bool connected() const {
        std::lock_guard lock(mutex_);
        return playerId_ != 0 && !stopped_;
    }

    std::uint32_t playerId() const {
        std::lock_guard lock(mutex_);
        return playerId_;
    }

    std::string error() const {
        std::lock_guard lock(mutex_);
        return failure_;
    }

    std::optional<WorldSnapshot> receive() {
        std::lock_guard lock(mutex_);
        return std::exchange(latest_, std::nullopt);
    }

    bool send(InputCommand input) {
        if (!input.valid()) {
            return false;
        }
        auto data = wire::input(input);
        std::lock_guard lock(mutex_);
        if (stopped_ || playerId_ == 0 || input.playerId != playerId_ ||
            queuedInputs_ >= maximumQueuedInputs) {
            return false;
        }

        ++queuedInputs_;
        try {
            asio::post(io_, [this, data = std::move(data)] {
                {
                    std::lock_guard queueLock(mutex_);
                    --queuedInputs_;
                }
                if (channel_) {
                    channel_->send(data);
                }
            });
        } catch (...) {
            --queuedInputs_; // A failed post must not consume a queue slot.
            throw;
        }
        return true;
    }

    // Called by the one consumer thread; all callbacks are drained before destruction.
    void stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopped_) {
                return;
            }
            stopped_ = true;
            playerId_ = 0;
        }
        asio::post(io_, [this] {
            fail("Client closed");
            work_.reset();
        });
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    static constexpr std::size_t maximumQueuedInputs = 64;
    static constexpr auto handshakeTimeout = std::chrono::seconds(5);

    void beginConnection(const std::string& host, std::uint16_t port) {
        deadline_.expires_after(handshakeTimeout);
        deadline_.async_wait([this](asio::error_code error) {
            if (!error) {
                fail("Connect/handshake timeout");
            }
        });
        resolver_.async_resolve(
            host, std::to_string(port),
            [this](asio::error_code error, asio::ip::tcp::resolver::results_type endpoints) {
                if (terminal_) {
                    return;
                }
                if (error) {
                    fail(error.message());
                    return;
                }
                connectSocket(endpoints);
            });
    }

    void connectSocket(const asio::ip::tcp::resolver::results_type& endpoints) {
        asio::async_connect(socket_, endpoints,
                            [this](asio::error_code error, const asio::ip::tcp::endpoint&) {
                                if (terminal_) {
                                    return;
                                }
                                if (error) {
                                    fail(error.message());
                                    return;
                                }
                                startHandshake();
                            });
    }

    void startHandshake() {
        channel_ = std::make_shared<TcpChannel>(std::move(socket_));
        channel_->start(
            [this](const wire::Bytes& data) {
                receiveMessage(data);
            },
            [this](std::string reason) {
                terminal_ = true;
                recordFailure(std::move(reason));
            });
        channel_->send(wire::clientHello());
    }

    void receiveMessage(const wire::Bytes& data) {
        std::lock_guard lock(mutex_);
        if (playerId_ == 0) {
            playerId_ = wire::readServerHello(data);
            deadline_.cancel();
            return;
        }
        auto state = wire::readSnapshot(data);
        if (state.tick > lastReceivedTick_) {
            lastReceivedTick_ = state.tick;
            latest_ = std::move(state);
        }
    }

    void recordFailure(std::string reason) {
        std::lock_guard lock(mutex_);
        if (failure_.empty()) {
            failure_ = std::move(reason);
        }
        playerId_ = 0;
    }

    void fail(std::string reason) {
        terminal_ = true;
        recordFailure(std::move(reason));
        resolver_.cancel();
        deadline_.cancel();
        asio::error_code ignored;
        socket_.close(ignored);
        if (channel_) {
            channel_->close("Connection failed");
        }
    }

    asio::io_context io_;
    asio::executor_work_guard<asio::io_context::executor_type> work_{io_.get_executor()};
    asio::ip::tcp::resolver resolver_{io_};
    asio::ip::tcp::socket socket_{io_};
    asio::steady_timer deadline_{io_};
    std::shared_ptr<TcpChannel> channel_;

    // The consumer and io thread share only these fields, always under mutex_.
    mutable std::mutex mutex_;
    std::uint32_t playerId_{};
    std::size_t queuedInputs_{};
    std::optional<WorldSnapshot> latest_;
    std::string failure_;
    bool stopped_{};

    // The io thread alone owns the connection lifecycle and snapshot ordering.
    std::uint64_t lastReceivedTick_{};
    bool terminal_{};
    std::thread thread_;
};

TcpClientTransport::TcpClientTransport(const std::string& host, std::uint16_t port)
    : impl_(std::make_unique<Impl>(host, port)) {}

TcpClientTransport::~TcpClientTransport() = default;

bool TcpClientTransport::connected() const {
    return impl_->connected();
}

std::uint32_t TcpClientTransport::playerId() const {
    return impl_->playerId();
}

bool TcpClientTransport::send(InputCommand input) {
    return impl_->send(input);
}

std::optional<WorldSnapshot> TcpClientTransport::receive() {
    return impl_->receive();
}

std::string TcpClientTransport::error() const {
    return impl_->error();
}

void TcpClientTransport::close() {
    impl_->stop();
}
} // namespace sandbox