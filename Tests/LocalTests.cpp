#include "Client/Transport/LocalTransport.h"
#include "TestSupport.h"
int main() { return test::run([] {
    auto server = std::make_shared<sandbox::ServerHost>();
    server->start();
    sandbox::LocalTransport a(server), b(server);
    CHECK(a.connected() && b.connected() && a.playerId() != b.playerId());
    CHECK(!a.send({b.playerId(),1,1,0}));
    CHECK(a.send({a.playerId(),1,1,0}));
    std::optional<sandbox::WorldSnapshot> state;
    CHECK(test::waitFor([&] { state=a.receive(); return state && state->players.size()==2 && state->players[0].velocity.x>0.5; }));
    CHECK(test::approx(state->players[0].angularVelocity.z,-state->players[0].velocity.x/0.5));
    CHECK(state->players[0].orientation!=sandbox::Quaternion{});
    CHECK(state->players[1].velocity.x == 0);
    a.close(); a.close(); CHECK(!a.connected());
    CHECK(test::waitFor([&] { return server->snapshot().players.size()==1; }));
    server->stop(); CHECK(!b.connected());
    server->start(); CHECK(!b.connected());
}); }
