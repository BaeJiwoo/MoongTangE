#include "Client/Transport/LocalTransport.h"
#include "TestSupport.h"
#include <memory>
#include <optional>

int main() {
    return MoongTangE::test::run([] {
        MOONGTANGE_CHECK(MoongTangE::test::throws([] {
            MoongTangE::LocalTransport invalid(nullptr);
        }));
        MOONGTANGE_CHECK(MoongTangE::test::throws([] {
            MoongTangE::LocalTransport invalid(std::make_shared<MoongTangE::ServerHost>());
        }));
        auto server = std::make_shared<MoongTangE::ServerHost>();
        server->start();
        MoongTangE::LocalTransport a(server), b(server);
        MOONGTANGE_CHECK(a.connected() && b.connected() && a.playerId() != b.playerId());
        MOONGTANGE_CHECK(!a.send({b.playerId(), 1, 1, 0}));
        MOONGTANGE_CHECK(a.send({a.playerId(), 1, 1, 0}));
        std::optional<MoongTangE::WorldSnapshot> state;
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            state = a.receive();
            return state && state->players.size() == 2 && state->players[0].velocity.x > 0.5;
        }));
        MOONGTANGE_CHECK(MoongTangE::test::approx(state->players[0].angularVelocity.z,
                                                  -state->players[0].velocity.x / 0.5));
        MOONGTANGE_CHECK(state->players[0].orientation != MoongTangE::Quaternion{});
        MOONGTANGE_CHECK(state->players[1].velocity.x == 0);
        a.close();
        a.close();
        MOONGTANGE_CHECK(!a.connected());
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            return server->snapshot().players.size() == 1;
        }));
        server->stop();
        MOONGTANGE_CHECK(!b.connected());
        server->start();
        MOONGTANGE_CHECK(!b.connected());
    });
}
