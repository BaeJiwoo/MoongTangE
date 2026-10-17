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

MoongTangE::wire::Bytes read_frame(asio::ip::tcp::socket& socket)
{
    std::array<std::uint8_t, wire::FRAME_HEADER_BYTES> header{};
    asio::read(socket, asio::buffer(header));
    MoongTangE::wire::Bytes data(MoongTangE::wire::frame_size(header));
    asio::read(socket, asio::buffer(data));
    return data;
}

bool closed(asio::ip::tcp::socket& socket)
{
    socket.non_blocking(true);
    return MoongTangE::test::wait_for([&] {
        std::array<char, 4096> bytes{};
        asio::error_code error;
        socket.read_some(asio::buffer(bytes), error);
        return error && error != asio::error::would_block && error != asio::error::try_again;
    });
}

void channel_starts_once_and_closes_once()
{
    {
        asio::io_context channelIo;
        auto channel = std::make_shared<TcpChannel>(asio::ip::tcp::socket(channelIo));
        unsigned closeCalls = 0;
        auto onMessage = [](const wire::Bytes&) {};
        auto onClose = [&](std::string) {
            ++closeCalls;
        };
        channel->Start(onMessage, onClose);
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            channel->Start(onMessage, onClose);
        }));
        channel->Close();
        channel->Close();
        channelIo.run();
        MOONGTANGE_CHECK(closeCalls == 1);
    }
}

void consumed_snapshots_cannot_rewind()
{
    asio::io_context io;
    asio::ip::tcp::acceptor listener(io, {asio::ip::make_address("127.0.0.1"), 0});
    TcpClientTransport client("127.0.0.1", listener.local_endpoint().port());
    auto peer = listener.accept();
    wire::read_client_hello(read_frame(peer));
    auto send = [&](const wire::Bytes& payload) {
        const auto bytes = wire::frame(payload);
        asio::write(peer, asio::buffer(bytes));
    };
    send(wire::server_hello(1));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Connected();
    }));
    send(wire::snapshot({42, {{1, {}, {}}}}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        const auto state = client.Receive();
        return state && state->tick == 42;
    }));
    send(wire::snapshot({41, {{1, {}, {}}}}));
    // TCP ordering makes this terminal message a barrier after the stale snapshot.
    send(wire::Bytes{255});
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return !client.Connected();
    }));
    MOONGTANGE_CHECK(!client.Receive());
}

void clients_receive_authoritative_rolling(const AsioServer& server, ServerHost& core)
{
    TcpClientTransport first("127.0.0.1", server.Port()), second("127.0.0.1", server.Port());
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return first.Connected() && second.Connected();
    }));
    const auto firstId = first.PlayerId();
    MOONGTANGE_CHECK(firstId && firstId != second.PlayerId());
    MOONGTANGE_CHECK(!first.Send({second.PlayerId(), 1, 1, 0}));
    MOONGTANGE_CHECK(first.Send({firstId, 1, 1, 0}));
    MOONGTANGE_CHECK(second.Send({second.PlayerId(), 1, 0, -1}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        if (auto state = first.Receive()) {
            for (const auto& player : state->players) {
                if (player.id == firstId && player.velocity.x > 0.5) {
                    MOONGTANGE_CHECK(MoongTangE::test::approx(player.angularVelocity.z,
                                                              -player.velocity.x / 0.5));
                    MOONGTANGE_CHECK(player.orientation != Quaternion{} &&
                                     player.orientation.Unit());
                    return true;
                }
            }
        }
        return false;
    }));
    first.Close();
    first.Close();
    second.Close();
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return core.Snapshot().players.empty();
    }));
}

void fragmented_frames_and_disconnect(const AsioServer& server)
{
    asio::io_context io;
    const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), server.Port());
    {
        asio::ip::tcp::socket raw(io);
        raw.connect(endpoint);
        const auto greeting = wire::frame(wire::client_hello());
        for (const auto byte : greeting) {
            asio::write(raw, asio::buffer(&byte, 1));
        }
        const auto id = wire::read_server_hello(read_frame(raw));
        MOONGTANGE_CHECK(id);
        const auto command = wire::frame(wire::input({id, 1, 1, 0}));
        for (const auto byte : command) {
            asio::write(raw, asio::buffer(&byte, 1));
        }
        bool moved = false;
        for (int i = 0; i < 40 && !moved; ++i) {
            const auto state = wire::read_snapshot(read_frame(raw));
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

void malformed_frames_are_rejected(const AsioServer& server)
{
    asio::io_context io;
    const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), server.Port());
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

void connection_failure_and_early_close(const AsioServer& server)
{
    asio::io_context io;
    {
        TcpClientTransport abandoned("127.0.0.1", server.Port());
        abandoned.Close();
    }
    asio::ip::tcp::acceptor reserve(io, {asio::ip::make_address("127.0.0.1"), 0});
    const auto unused = reserve.local_endpoint().port();
    reserve.close();
    TcpClientTransport refused("127.0.0.1", unused);
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return !refused.Error().empty();
    }));
    MOONGTANGE_CHECK(!refused.Connected());
}

void shutdown_with_active_clients(const std::shared_ptr<ServerHost>& core)
{
    for (int i = 0; i < 5; ++i) {
        auto shutting = std::make_unique<AsioServer>(core, "127.0.0.1", 0);
        TcpClientTransport first("127.0.0.1", shutting->Port()),
            second("127.0.0.1", shutting->Port());
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            return first.Connected() && second.Connected();
        }));
        shutting.reset();
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            return !first.Connected() && !second.Connected();
        }));
    }
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        channel_starts_once_and_closes_once();
        consumed_snapshots_cannot_rewind();
        auto core = std::make_shared<MoongTangE::ServerHost>();
        core->Start();
        MoongTangE::AsioServer server(core, "127.0.0.1", 0);
        clients_receive_authoritative_rolling(server, *core);
        fragmented_frames_and_disconnect(server);
        malformed_frames_are_rejected(server);
        connection_failure_and_early_close(server);
        shutdown_with_active_clients(core);
    });
}
