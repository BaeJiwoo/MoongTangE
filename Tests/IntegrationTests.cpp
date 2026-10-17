#include "Client/Transport/LocalTransport.h"
#include "Client/Transport/TcpClientTransport.h"
#include "Common/Game/GameWorld.h"
#include "Network/Protocol/Codec.h"
#include "Server/Network/AsioServer.h"
#include "TestSupport.h"

#include <cstdint>
#include <memory>

namespace {
MoongTangE::PlayerState exercise(MoongTangE::IClientTransport& client)
{
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return client.Connected();
    }));
    MoongTangE::PlayerState result;
    const auto id = client.PlayerId();
    auto settled = [&] {
        if (auto state = client.Receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.position.y == 0.5) {
                    result = p;
                    return true;
                }
            }
        }
        return false;
    };
    MOONGTANGE_CHECK(MoongTangE::test::wait_for(settled));
    MOONGTANGE_CHECK(client.Send({id, 1, 1, -1}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        if (auto state = client.Receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity.x > 0) {
                    MOONGTANGE_CHECK(p.velocity.Length() < 5.5);
                    MOONGTANGE_CHECK(MoongTangE::test::approx(p.velocity.x, -p.velocity.z));
                    MOONGTANGE_CHECK(
                        MoongTangE::test::approx(p.angularVelocity.z, -p.velocity.x / 0.5));
                    MOONGTANGE_CHECK(
                        MoongTangE::test::approx(p.angularVelocity.x, p.velocity.z / 0.5));
                    MOONGTANGE_CHECK(p.orientation != MoongTangE::Quaternion{} &&
                                     p.orientation.Unit());
                    return true;
                }
            }
        }
        return false;
    }));
    MOONGTANGE_CHECK(client.Send({id, 2, 0, 0}));
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        if (auto state = client.Receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity == MoongTangE::Vector3{}) {
                    result = p;
                    return true;
                }
            }
        }
        return false;
    }));
    return result;
}

void deterministic_transport_equivalence()
{
    using namespace MoongTangE;
    // Identical input schedule: serialize/decode the TCP path, then compare every tick.
    GameWorld direct, serialized;
    MOONGTANGE_CHECK(direct.AddPlayer(1));
    MOONGTANGE_CHECK(serialized.AddPlayer(1));
    for (std::uint64_t tick = 0; tick < 600; ++tick) {
        InputCommand input{1, tick + 1, tick < 200 ? 1.0 : -1.0, tick % 120 < 60 ? 1.0 : 0.0};
        MOONGTANGE_CHECK(direct.Apply(input));
        MOONGTANGE_CHECK(serialized.Apply(wire::read_input(wire::input(input), 1)));
        direct.Step();
        serialized.Step();
        auto a = direct.Snapshot(), b = wire::read_snapshot(wire::snapshot(serialized.Snapshot()));
        MOONGTANGE_CHECK(a.tick == b.tick);
        MOONGTANGE_CHECK(a.players[0].position == b.players[0].position);
        MOONGTANGE_CHECK(a.players[0].velocity == b.players[0].velocity);
        MOONGTANGE_CHECK(a.players[0].orientation == b.players[0].orientation);
        MOONGTANGE_CHECK(a.players[0].angularVelocity == b.players[0].angularVelocity);
    }
}

void asynchronous_transport_invariants()
{
    using namespace MoongTangE;
    // Real asynchronous transports: compare physical invariants, allowing arrival-time
    // differences.
    auto localCore = std::make_shared<ServerHost>(), tcpCore = std::make_shared<ServerHost>();
    localCore->Start();
    tcpCore->Start();
    LocalTransport local(localCore);
    AsioServer server(tcpCore, "127.0.0.1", 0);
    TcpClientTransport tcp("127.0.0.1", server.Port());
    const auto a = exercise(local), b = exercise(tcp);
    MOONGTANGE_CHECK(a.position.y == b.position.y && a.velocity == b.velocity);
    MOONGTANGE_CHECK(a.angularVelocity == Vector3{} && b.angularVelocity == Vector3{});
    MOONGTANGE_CHECK(a.position.x > 0 && b.position.x > 0 && a.position.z < 0 && b.position.z < 0);
    local.Close();
    tcp.Close();
    MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
        return localCore->Snapshot().players.empty() && tcpCore->Snapshot().players.empty();
    }));
}
} // namespace

int main()
{
    return MoongTangE::test::run([] {
        deterministic_transport_equivalence();
        asynchronous_transport_invariants();
    });
}
