#include "Client/Transport/TcpClientTransport.h"
#include "Network/TcpChannel.h"
#include "Server/Network/AsioServer.h"
#include "TestSupport.h"
#include <array>
#include <asio.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace MoongTangE;

MoongTangE::wire::Bytes readFrame(asio::ip::tcp::socket& socket) {
    std::array<std::uint8_t, wire::FRAME_HEADER_BYTES> header{};
    asio::read(socket, asio::buffer(header));
    MoongTangE::wire::Bytes data(MoongTangE::wire::frameSize(header));
    asio::read(socket, asio::buffer(data));
    return data;
}

bool closed(asio::ip::tcp::socket& socket) {
    socket.non_blocking(true);
    return MoongTangE::test::waitFor([&] {
        std::array<char, 4096> bytes{};
        asio::error_code error;
        socket.read_some(asio::buffer(bytes), error);
        return error && error != asio::error::would_block && error != asio::error::try_again;
    });
}

void channelStartsOnceAndClosesOnce() {
    {
        asio::io_context channelIo;
        auto channel = std::make_shared<TcpChannel>(asio::ip::tcp::socket(channelIo));
        unsigned closeCalls = 0;
        auto onMessage = [](const wire::Bytes&) {};
        auto onClose = [&](std::string) {
            ++closeCalls;
        };
        channel->start(onMessage, onClose);
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            channel->start(onMessage, onClose);
        }));
        channel->close();
        channel->close();
        channelIo.run();
        MOONGTANGE_CHECK(closeCalls == 1);
    }
}

void consumedSnapshotsCannotRewind() {
    asio::io_context io;
    asio::ip::tcp::acceptor listener(io, {asio::ip::make_address("127.0.0.1"), 0});
    TcpClientTransport client("127.0.0.1", listener.local_endpoint().port());
    auto peer = listener.accept();
    wire::readClientHello(readFrame(peer));
    auto send = [&](const wire::Bytes& payload) {
        const auto bytes = wire::frame(payload);
        asio::write(peer, asio::buffer(bytes));
    };
    send(wire::serverHello(1));
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return client.connected();
    }));
    send(wire::snapshot({42, {{1, {}, {}}}}));
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        const auto state = client.receive();
        return state && state->tick == 42;
    }));
    send(wire::snapshot({41, {{1, {}, {}}}}));
    // TCP ordering makes this terminal message a barrier after the stale snapshot.
    send(wire::Bytes{255});
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return !client.connected();
    }));
    MOONGTANGE_CHECK(!client.receive());
}

void clientsReceiveAuthoritativeRolling(const AsioServer& server, ServerHost& core) {
    TcpClientTransport first("127.0.0.1", server.port()), second("127.0.0.1", server.port());
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return first.connected() && second.connected();
    }));
    const auto firstId = first.playerId();
    MOONGTANGE_CHECK(firstId && firstId != second.playerId());
    MOONGTANGE_CHECK(!first.send({second.playerId(), 1, 1, 0}));
    MOONGTANGE_CHECK(first.send({firstId, 1, 1, 0}));
    MOONGTANGE_CHECK(second.send({second.playerId(), 1, 0, -1}));
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        if (auto state = first.receive()) {
            for (const auto& player : state->players) {
                if (player.id == firstId && player.velocity.x > 0.5) {
                    MOONGTANGE_CHECK(MoongTangE::test::approx(player.angularVelocity.z,
                                                              -player.velocity.x / 0.5));
                    MOONGTANGE_CHECK(player.orientation != Quaternion{} &&
                                     player.orientation.unit());
                    return true;
                }
            }
        }
        return false;
    }));
    first.close();
    first.close();
    second.close();
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return core.snapshot().players.empty();
    }));
}

void fragmentedFramesAndDisconnect(const AsioServer& server) {
    asio::io_context io;
    const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), server.port());
    {
        asio::ip::tcp::socket raw(io);
        raw.connect(endpoint);
        const auto greeting = wire::frame(wire::clientHello());
        for (const auto byte : greeting) {
            asio::write(raw, asio::buffer(&byte, 1));
        }
        const auto id = wire::readServerHello(readFrame(raw));
        MOONGTANGE_CHECK(id);
        const auto command = wire::frame(wire::input({id, 1, 1, 0}));
        for (const auto byte : command) {
            asio::write(raw, asio::buffer(&byte, 1));
        }
        bool moved = false;
        for (int i = 0; i < 40 && !moved; ++i) {
            const auto state = wire::readSnapshot(readFrame(raw));
            for (const auto& p : state.players) {
                if (p.id == id && p.velocity.x > 0.5 && p.angularVelocity.z < 0) {
                    moved = true;
                }
            }
        }
        MOONGTANGE_CHECK(moved);
        const auto goodbye = wire::frame(wire::disconnect());
        asio::write(raw, asio::buffer(goodbye));
        MOONGTANGE_CHECK(closed(raw));
    }
}

void malformedFramesAreRejected(const AsioServer& server) {
    asio::io_context io;
    const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), server.port());
    for (const auto& bad : std::vector<wire::Bytes>{{0, 0, 32, 1},
                                                    {0, 0, 0, 0},
                                                    wire::frame(wire::Bytes{1, 0, 1}),
                                                    wire::frame(wire::input({1, 1, 0, 0}))}) {
        asio::ip::tcp::socket raw(io);
        raw.connect(endpoint);
        asio::write(raw, asio::buffer(bad));
        MOONGTANGE_CHECK(closed(raw));
    }
}

void connectionFailureAndEarlyClose(const AsioServer& server) {
    asio::io_context io;
    {
        TcpClientTransport abandoned("127.0.0.1", server.port());
        abandoned.close();
    }
    asio::ip::tcp::acceptor reserve(io, {asio::ip::make_address("127.0.0.1"), 0});
    const auto unused = reserve.local_endpoint().port();
    reserve.close();
    TcpClientTransport refused("127.0.0.1", unused);
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return !refused.error().empty();
    }));
    MOONGTANGE_CHECK(!refused.connected());
}

void shutdownWithActiveClients(const std::shared_ptr<ServerHost>& core) {
    for (int i = 0; i < 5; ++i) {
        auto shutting = std::make_unique<AsioServer>(core, "127.0.0.1", 0);
        TcpClientTransport first("127.0.0.1", shutting->port()),
            second("127.0.0.1", shutting->port());
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            return first.connected() && second.connected();
        }));
        shutting.reset();
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            return !first.connected() && !second.connected();
        }));
    }
}

} // namespace

int main() {
    return MoongTangE::test::run([] {
        channelStartsOnceAndClosesOnce();
        consumedSnapshotsCannotRewind();
        auto core = std::make_shared<MoongTangE::ServerHost>();
        core->start();
        MoongTangE::AsioServer server(core, "127.0.0.1", 0);
        clientsReceiveAuthoritativeRolling(server, *core);
        fragmentedFramesAndDisconnect(server);
        malformedFramesAreRejected(server);
        connectionFailureAndEarlyClose(server);
        shutdownWithActiveClients(core);
    });
}
