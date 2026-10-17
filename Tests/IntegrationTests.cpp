#include "Client/Transport/LocalTransport.h"
#include "Client/Transport/TcpClientTransport.h"
#include "Common/Game/GameWorld.h"
#include "Common/Protocol/Wire.h"
#include "Server/Network/AsioServer.h"
#include "TestSupport.h"
#include <cstdint>
#include <memory>

namespace {
sandbox::PlayerState exercise(sandbox::IClientTransport& client) {
    CHECK(test::waitFor([&] {
        return client.connected();
    }));
    sandbox::PlayerState result;
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
    CHECK(test::waitFor(settled));
    CHECK(client.send({id, 1, 1, -1}));
    CHECK(test::waitFor([&] {
        if (auto state = client.receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity.x > 0) {
                    CHECK(p.velocity.length() < 5.5);
                    CHECK(test::approx(p.velocity.x, -p.velocity.z));
                    CHECK(test::approx(p.angularVelocity.z, -p.velocity.x / 0.5));
                    CHECK(test::approx(p.angularVelocity.x, p.velocity.z / 0.5));
                    CHECK(p.orientation != sandbox::Quaternion{} && p.orientation.unit());
                    return true;
                }
            }
        }
        return false;
    }));
    CHECK(client.send({id, 2, 0, 0}));
    CHECK(test::waitFor([&] {
        if (auto state = client.receive()) {
            for (const auto& p : state->players) {
                if (p.id == id && p.velocity == sandbox::Vector3{}) {
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
    using namespace sandbox;
    // Identical input schedule: serialize/decode the TCP path, then compare every tick.
    GameWorld direct, serialized;
    CHECK(direct.addPlayer(1));
    CHECK(serialized.addPlayer(1));
    for (std::uint64_t tick = 0; tick < 600; ++tick) {
        InputCommand input{1, tick + 1, tick < 200 ? 1.0 : -1.0, tick % 120 < 60 ? 1.0 : 0.0};
        CHECK(direct.apply(input));
        CHECK(serialized.apply(wire::readInput(wire::input(input), 1)));
        direct.step();
        serialized.step();
        auto a = direct.snapshot(), b = wire::readSnapshot(wire::snapshot(serialized.snapshot()));
        CHECK(a.tick == b.tick);
        CHECK(a.players[0].position == b.players[0].position);
        CHECK(a.players[0].velocity == b.players[0].velocity);
        CHECK(a.players[0].orientation == b.players[0].orientation);
        CHECK(a.players[0].angularVelocity == b.players[0].angularVelocity);
    }
}

void asynchronousTransportInvariants() {
    using namespace sandbox;
    // Real asynchronous transports: compare physical invariants, allowing arrival-time
    // differences.
    auto localCore = std::make_shared<ServerHost>(), tcpCore = std::make_shared<ServerHost>();
    localCore->start();
    tcpCore->start();
    LocalTransport local(localCore);
    AsioServer server(tcpCore, "127.0.0.1", 0);
    TcpClientTransport tcp("127.0.0.1", server.port());
    const auto a = exercise(local), b = exercise(tcp);
    CHECK(a.position.y == b.position.y && a.velocity == b.velocity);
    CHECK(a.angularVelocity == Vector3{} && b.angularVelocity == Vector3{});
    CHECK(a.position.x > 0 && b.position.x > 0 && a.position.z < 0 && b.position.z < 0);
    local.close();
    tcp.close();
    CHECK(test::waitFor([&] {
        return localCore->snapshot().players.empty() && tcpCore->snapshot().players.empty();
    }));
}
} // namespace

int main() {
    return test::run([] {
        deterministicTransportEquivalence();
        asynchronousTransportInvariants();
    });
}
