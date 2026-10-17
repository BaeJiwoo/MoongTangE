#include "Server/Network/AsioServer.h"
#include "Network/TcpChannel.h"
#include <map>

namespace sandbox {
struct AsioServer::Impl {
    std::shared_ptr<ServerHost> host;
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor;
    asio::steady_timer publish;
    std::map<std::shared_ptr<TcpChannel>, std::uint32_t> sessions;
    std::thread thread;
    std::uint64_t lastTick{};
    std::uint16_t boundPort{};
    bool stopping{};

    Impl(std::shared_ptr<ServerHost> h, const std::string& address, std::uint16_t port)
        : host(std::move(h)), acceptor(io), publish(io) {
        if (!host || !host->running()) {
            throw std::invalid_argument("ServerCore is not running");
        }
        const asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);
        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();
        boundPort = acceptor.local_endpoint().port();
        accept();
        broadcast();
        thread = std::thread([this] {
            io.run();
        });
    }

    ~Impl() {
        asio::post(io, [this] {
            stopping = true;
            asio::error_code ignored;
            acceptor.close(ignored);
            publish.cancel();
            const auto copy = sessions;
            for (const auto& [channel, id] : copy) {
                (void)id;
                channel->close("Server stopping");
            }
        });
        if (thread.joinable()) {
            thread.join();
        }
    }

    void accept() {
        acceptor.async_accept([this](asio::error_code error, asio::ip::tcp::socket socket) {
            if (stopping) {
                return;
            }
            if (!error) {
                if (sessions.size() < maxPlayers * 2) {
                    auto channel = std::make_shared<TcpChannel>(std::move(socket));
                    sessions.emplace(channel, 0);
                    std::weak_ptr<TcpChannel> weak = channel;
                    channel->onMessage = [this, weak](const wire::Bytes& data) {
                        if (auto c = weak.lock()) {
                            message(c, data);
                        }
                    };
                    channel->onClose = [this, weak](std::string) {
                        if (auto c = weak.lock()) {
                            const auto it = sessions.find(c);
                            if (it != sessions.end()) {
                                host->disconnect(it->second);
                                sessions.erase(it);
                            }
                        }
                    };
                    channel->start();
                }
            }
            if (acceptor.is_open()) {
                accept();
            }
        });
    }

    void message(const std::shared_ptr<TcpChannel>& channel, const wire::Bytes& data) {
        auto& id = sessions.at(channel);
        if (!id) {
            wire::readHello(data, false);
            id = host->connect();
            wire::require(id != 0);
            channel->send(wire::hello(id));
        } else if (wire::type(data) == wire::Type::Input) {
            wire::require(host->submit(wire::readInput(data, id)));
        } else if (wire::isDisconnect(data)) {
            channel->close("Client disconnected");
        } else {
            channel->close("Unexpected message");
        }
    }

    void broadcast() {
        publish.expires_after(std::chrono::milliseconds(50));
        publish.async_wait([this](asio::error_code error) {
            if (error || stopping) {
                return;
            }
            const auto state = host->snapshot();
            if (state.tick && state.tick != lastTick) {
                lastTick = state.tick;
                const auto bytes = wire::snapshot(state);
                const auto copy = sessions;
                for (const auto& [channel, id] : copy) {
                    if (id) {
                        channel->send(bytes);
                    }
                }
            }
            broadcast();
        });
    }
};

AsioServer::AsioServer(std::shared_ptr<ServerHost> host, const std::string& address,
                       std::uint16_t port)
    : impl_(std::make_unique<Impl>(std::move(host), address, port)) {}

AsioServer::~AsioServer() = default;

std::uint16_t AsioServer::port() const {
    return impl_->boundPort;
}
} // namespace sandbox
