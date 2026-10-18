#include "Client/Transport/LocalTransport.h"
#include "Client/Transport/TcpClientTransport.h"
#include "Common/Game/GameWorld.h"
#include "Network/Protocol/Codec.h"
#include "Server/Network/AsioServer.h"
#include "TestSupport.h"
#include <cstdint>
#include <memory>

namespace {
MoongTangE::PlayerState exercise(MoongTangE::IClientTransport& client) {
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return client.connected();
    }));
    MoongTangE::PlayerState result;
    const auto id = client.playerId();
    auto settled = [&] {
        if (auto state = client.receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.position.y == 0.5) {
                    result = p;
                    return true;
                }
            }
        }
        return false;
    };
    MOONGTANGE_CHECK(MoongTangE::test::waitFor(settled));
    MOONGTANGE_CHECK(client.send({id, 1, 1, -1}));
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        if (auto state = client.receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity.x > 0) {
                    MOONGTANGE_CHECK(p.velocity.length() < 5.5);
                    MOONGTANGE_CHECK(MoongTangE::test::approx(p.velocity.x, -p.velocity.z));
                    MOONGTANGE_CHECK(
                        MoongTangE::test::approx(p.angularVelocity.z, -p.velocity.x / 0.5));
                    MOONGTANGE_CHECK(
                        MoongTangE::test::approx(p.angularVelocity.x, p.velocity.z / 0.5));
                    MOONGTANGE_CHECK(p.orientation != MoongTangE::Quaternion{} &&
                                     p.orientation.unit());
                    return true;
                }
            }
        }
        return false;
    }));
    MOONGTANGE_CHECK(client.send({id, 2, 0, 0}));
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        if (auto state = client.receive()) {
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

void deterministicTransportEquivalence() {
    using namespace MoongTangE;
    // Identical input schedule: serialize/decode the TCP path, then compare every tick.
    GameWorld direct, serialized;
    MOONGTANGE_CHECK(direct.addPlayer(1));
    MOONGTANGE_CHECK(serialized.addPlayer(1));
    for (std::uint64_t tick = 0; tick < 600; ++tick) {
        InputCommand input{1, tick + 1, tick < 200 ? 1.0 : -1.0, tick % 120 < 60 ? 1.0 : 0.0};
        MOONGTANGE_CHECK(direct.apply(input));
        MOONGTANGE_CHECK(serialized.apply(wire::readInput(wire::input(input), 1)));
        direct.step();
        serialized.step();
        auto a = direct.snapshot(), b = wire::readSnapshot(wire::snapshot(serialized.snapshot()));
        MOONGTANGE_CHECK(a.tick == b.tick);
        MOONGTANGE_CHECK(a.players[0].position == b.players[0].position);
        MOONGTANGE_CHECK(a.players[0].velocity == b.players[0].velocity);
        MOONGTANGE_CHECK(a.players[0].orientation == b.players[0].orientation);
        MOONGTANGE_CHECK(a.players[0].angularVelocity == b.players[0].angularVelocity);
    }
}

void asynchronousTransportInvariants() {
    using namespace MoongTangE;
    // Real asynchronous transports: compare physical invariants, allowing arrival-time
    // differences.
    auto localCore = std::make_shared<ServerHost>(), tcpCore = std::make_shared<ServerHost>();
    localCore->start();
    tcpCore->start();
    LocalTransport local(localCore);
    AsioServer server(tcpCore, "127.0.0.1", 0);
    TcpClientTransport tcp("127.0.0.1", server.port());
    const auto a = exercise(local), b = exercise(tcp);
    MOONGTANGE_CHECK(a.position.y == b.position.y && a.velocity == b.velocity);
    MOONGTANGE_CHECK(a.angularVelocity == Vector3{} && b.angularVelocity == Vector3{});
    MOONGTANGE_CHECK(a.position.x > 0 && b.position.x > 0 && a.position.z < 0 && b.position.z < 0);
    local.close();
    tcp.close();
    MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
        return localCore->snapshot().players.empty() && tcpCore->snapshot().players.empty();
    }));
}
} // namespace

int main() {
    return MoongTangE::test::run([] {
        deterministicTransportEquivalence();
        asynchronousTransportInvariants();
    });
}
