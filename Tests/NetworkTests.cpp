#include "Client/Transport/TcpClientTransport.h"
#include "Network/Protocol/Codec.h"
#include "Server/Network/NetworkServer.h"
#include "TcpTestSocket.h"
#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace MoongTangE;

MoongTangE::wire::Bytes read_frame(MoongTangE::test::TcpTestSocket& socket)
{
    std::array<std::uint8_t, wire::FRAME_HEADER_BYTES> header{};
    socket.ReadExact(header);

    MoongTangE::wire::Bytes data(MoongTangE::wire::frame_size(header));
    socket.ReadExact(data);
    return data;
}

bool closed(const MoongTangE::test::TcpTestSocket& socket)
{
    return MoongTangE::test::wait_for([&] {
        return socket.Closed();
    });
}

void consumed_snapshots_cannot_rewind()
{
    test::TcpTestListener listener;
    TcpClientTransport client("127.0.0.1", listener.Port());
    auto peer = listener.Accept();

    wire::read_client_hello(read_frame(peer));
    auto send = [&](const wire::Bytes& payload) {
        const auto bytes = wire::frame(payload);
        peer.Write(bytes);
    };

    send(wire::server_hello({AdmissionStatus::ACCEPTED, 1, 7}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Connected();
    }));

    send(wire::snapshot({7, {42, {{1, {}, {}, {}, {}, {}}}, {}}}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        const auto state = client.Receive();
        return state && state->tick == 42;
    }));

    send(wire::snapshot({7, {41, {{1, {}, {}, {}, {}, {}}}, {}}}));
    // TCP ordering makes this terminal message a barrier after the stale snapshot.
    send(wire::Bytes{255});
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return !client.Connected();
    }));
    MOONGTANGE_CHECK(!client.Receive());
}

void client_close_sends_disconnect_before_socket_shutdown()
{
    test::TcpTestListener listener;
    TcpClientTransport client("127.0.0.1", listener.Port());
    auto peer = listener.Accept();

    wire::read_client_hello(read_frame(peer));
    const auto greeting = wire::frame(wire::server_hello({AdmissionStatus::ACCEPTED, 1, 7}));
    peer.Write(greeting);
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Connected();
    }));

    client.Close();
    client.Close();
    MOONGTANGE_CHECK(wire::is_disconnect(read_frame(peer)));
    MOONGTANGE_CHECK(closed(peer));
    MOONGTANGE_CHECK(!client.Connected() && client.Error().empty());
}

void clients_receive_authoritative_rolling(const NetworkServer& server, ServerHost& core)
{
    TcpClientTransport first("127.0.0.1", server.Port(), {}, "Alice"),
        second("127.0.0.1", server.Port(), {}, "Bob");
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
                    MOONGTANGE_CHECK(player.nickname == "Alice");
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
        return std::ranges::all_of(core.Snapshots(), [](const auto& room) {
            return room.world.players.empty();
        });
    }));
}

void invalid_nickname_is_rejected(const NetworkServer& server)
{
    auto client = test::TcpTestSocket::Connect("127.0.0.1", server.Port());
    const auto greeting = wire::frame(wire::client_hello({}, ""));
    client.Write(greeting);

    const auto admission = wire::read_server_hello(read_frame(client));
    MOONGTANGE_CHECK(admission.status == AdmissionStatus::INVALID_NICKNAME);
    MOONGTANGE_CHECK(admission.playerId == 0 && admission.roomId == 0);
    MOONGTANGE_CHECK(closed(client));

    TcpClientTransport invalid("127.0.0.1", server.Port(), {}, "");
    MOONGTANGE_CHECK(test::wait_for([&] {
        return invalid.Error() == "Enter a valid nickname before joining multiplayer";
    }));
}

void clients_reject_foreign_rooms_and_report_full()
{
    for (const bool full : {false, true}) {
        test::TcpTestListener listener;
        TcpClientTransport client("127.0.0.1", listener.Port());
        auto peer = listener.Accept();
        wire::read_client_hello(read_frame(peer));
        const auto greeting = wire::frame(
            wire::server_hello(full ? AdmissionResult{AdmissionStatus::FULL, 0, 0}
                                    : AdmissionResult{AdmissionStatus::ACCEPTED, 1, 7}));
        peer.Write(greeting);
        if (!full) {
            MOONGTANGE_CHECK(test::wait_for([&] {
                return client.RoomId() == 7;
            }));
            const auto foreign =
                wire::frame(wire::snapshot({8, {42, {{1, {}, {}, {}, {}, {}}}, {}}}));
            peer.Write(foreign);
        }
        MOONGTANGE_CHECK(test::wait_for([&] {
            return !client.Error().empty();
        }));
        MOONGTANGE_CHECK(!client.Connected() && client.RoomId() == 0 && !client.Receive());
        if (full) {
            MOONGTANGE_CHECK(client.Error() == "All game rooms are full");
        }
    }
}

void fragmented_frames_and_disconnect(const NetworkServer& server)
{
    {
        auto raw = test::TcpTestSocket::Connect("127.0.0.1", server.Port());

        const auto greeting = wire::frame(wire::client_hello({}, "RawPlayer"));
        for (const auto byte : greeting) {
            raw.WriteByte(byte);
        }
        const auto id = wire::read_server_hello(read_frame(raw)).playerId;
        MOONGTANGE_CHECK(id);

        const auto command = wire::frame(wire::input({id, 1, 1, 0}));
        for (const auto byte : command) {
            raw.WriteByte(byte);
        }

        bool moved = false;
        for (int i = 0; i < 40 && !moved; ++i) {
            const auto state = wire::read_snapshot(read_frame(raw)).world;
            for (const auto& p : state.players) {
                if (p.id == id && p.velocity.x > 0.5 && p.angularVelocity.z < 0) {
                    moved = true;
                }
            }
        }
        MOONGTANGE_CHECK(moved);

        const auto goodbye = wire::frame(wire::disconnect());
        raw.Write(goodbye);
        MOONGTANGE_CHECK(closed(raw));
    }
}

void malformed_frames_are_rejected(const NetworkServer& server)
{
    for (const auto& bad : std::vector<wire::Bytes>{{0, 0, 32, 1},
                                                    {0, 0, 0, 0},
                                                    wire::frame(wire::Bytes{1, 0, 1}),
                                                    wire::frame(wire::input({1, 1, 0, 0}))}) {
        auto raw = test::TcpTestSocket::Connect("127.0.0.1", server.Port());
        raw.Write(bad);
        MOONGTANGE_CHECK(closed(raw));
    }
}

void connection_failure_and_early_close(const NetworkServer& server)
{
    {
        TcpClientTransport abandoned("127.0.0.1", server.Port());
        abandoned.Close();
    }

    test::TcpTestListener reserve;
    const auto unused = reserve.Port();
    reserve.Close();

    TcpClientTransport refused("127.0.0.1", unused);
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return !refused.Error().empty();
    }));
    MOONGTANGE_CHECK(!refused.Connected());
}

void shutdown_with_active_clients(const std::shared_ptr<ServerHost>& core)
{
    for (int i = 0; i < 5; ++i) {
        auto shutting = std::make_unique<NetworkServer>(core, "127.0.0.1", 0);
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
        consumed_snapshots_cannot_rewind();
        client_close_sends_disconnect_before_socket_shutdown();
        clients_reject_foreign_rooms_and_report_full();

        auto core = std::make_shared<MoongTangE::ServerHost>();
        core->Start();
        MoongTangE::NetworkServer server(core, "127.0.0.1", 0);

        clients_receive_authoritative_rolling(server, *core);
        invalid_nickname_is_rejected(server);
        fragmented_frames_and_disconnect(server);
        malformed_frames_are_rejected(server);
        connection_failure_and_early_close(server);
        shutdown_with_active_clients(core);
    });
}
