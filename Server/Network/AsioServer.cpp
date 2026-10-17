#include "Server/Network/AsioServer.h"
#include "Common/Protocol/Wire.h"
#include "Common/SimulationConfig.h"
#include "Network/TcpChannel.h"
#include <asio.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace sandbox {
class AsioServer::Impl {
public:
    Impl(std::shared_ptr<ServerHost> host, const std::string& address, std::uint16_t port)
        : host_(std::move(host)), acceptor_(io_), publishTimer_(io_) {
        if (!host_ || !host_->running()) {
            throw std::invalid_argument("ServerCore is not running");
        }
        const asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.bind(endpoint);
        acceptor_.listen();
        boundPort_ = acceptor_.local_endpoint().port();
        acceptNext();
        scheduleBroadcast();
        thread_ = std::thread([this] {
            io_.run();
        });
    }

    ~Impl() {
        asio::post(io_, [this] {
            closeSessions();
        });
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    std::uint16_t port() const {
        return boundPort_;
    }

private:
    static constexpr std::size_t maximumConnections = maxPlayers * 2;
    static constexpr auto publishInterval = std::chrono::milliseconds(
        1000 * simulation::snapshotIntervalTicks / simulation::ticksPerSecond);

    void closeSessions() {
        stopping_ = true;
        asio::error_code ignored;
        acceptor_.close(ignored);
        publishTimer_.cancel();
        // Closing invokes removal callbacks; iterate a snapshot, not the modified map.
        const auto sessions = sessions_;
        for (const auto& [channel, playerId] : sessions) {
            (void)playerId;
            channel->close("Server stopping");
        }
    }

    void acceptNext() {
        acceptor_.async_accept([this](asio::error_code error, asio::ip::tcp::socket socket) {
            if (stopping_) {
                return;
            }
            if (!error && sessions_.size() < maximumConnections) {
                addSession(std::move(socket));
            }
            if (acceptor_.is_open()) {
                acceptNext();
            }
        });
    }

    void addSession(asio::ip::tcp::socket socket) {
        auto channel = std::make_shared<TcpChannel>(std::move(socket));
        sessions_.emplace(channel, 0);
        // Weak callbacks avoid a channel -> callback -> channel ownership cycle.
        const std::weak_ptr<TcpChannel> weakChannel = channel;
        channel->start(
            [this, weakChannel](const wire::Bytes& data) {
                if (auto connection = weakChannel.lock()) {
                    receiveMessage(connection, data);
                }
            },
            [this, weakChannel](std::string) {
                if (auto connection = weakChannel.lock()) {
                    removeSession(connection);
                }
            });
    }

    void removeSession(const std::shared_ptr<TcpChannel>& channel) {
        const auto session = sessions_.find(channel);
        if (session != sessions_.end()) {
            host_->disconnect(session->second);
            sessions_.erase(session);
        }
    }

    void receiveMessage(const std::shared_ptr<TcpChannel>& channel, const wire::Bytes& data) {
        auto& playerId = sessions_.at(channel);
        if (playerId == 0) {
            wire::readClientHello(data);
            playerId = host_->connect();
            wire::require(playerId != 0);
            channel->send(wire::serverHello(playerId));
        } else if (wire::type(data) == wire::Type::Input) {
            wire::require(host_->submit(wire::readInput(data, playerId)));
        } else if (wire::isDisconnect(data)) {
            channel->close("Client disconnected");
        } else {
            channel->close("Unexpected message");
        }
    }

    void broadcastLatest() {
        const auto state = host_->snapshot();
        if (state.tick == 0 || state.tick == lastPublishedTick_) {
            return;
        }
        lastPublishedTick_ = state.tick;
        const auto bytes = wire::snapshot(state);
        // send() can disconnect a slow peer and remove it from sessions_.
        const auto sessions = sessions_;
        for (const auto& [channel, playerId] : sessions) {
            if (playerId != 0) {
                channel->send(bytes);
            }
        }
    }

    void scheduleBroadcast() {
        publishTimer_.expires_after(publishInterval);
        publishTimer_.async_wait([this](asio::error_code error) {
            if (error || stopping_) {
                return;
            }
            broadcastLatest();
            scheduleBroadcast();
        });
    }

    std::shared_ptr<ServerHost> host_;
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    asio::steady_timer publishTimer_;
    // Everything below except the immutable port is owned by the io thread.
    std::map<std::shared_ptr<TcpChannel>, std::uint32_t> sessions_;
    std::uint64_t lastPublishedTick_{};
    std::uint16_t boundPort_{};
    bool stopping_{};
    std::thread thread_;
};

AsioServer::AsioServer(std::shared_ptr<ServerHost> host, const std::string& address,
                       std::uint16_t port)
    : impl_(std::make_unique<Impl>(std::move(host), address, port)) {}

AsioServer::~AsioServer() = default;

std::uint16_t AsioServer::port() const {
    return impl_->port();
}
} // namespace sandbox