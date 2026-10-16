#include "Common/Game/GameWorld.h"
#include "TestSupport.h"
#include <limits>
int main() { return test::run([] {
    sandbox::GameWorld world;
    CHECK(world.addPlayer(1)); CHECK(world.addPlayer(2));
    CHECK(!world.addPlayer(1)); CHECK(!world.addPlayer(0));
    CHECK(world.apply({1,1,1,1}));
    CHECK(!world.apply({1,1,-1,-1})); CHECK(!world.apply({3,1,1,0}));
    CHECK(!world.apply({1,2,2,0}));
    CHECK(!world.apply({1,2,std::numeric_limits<double>::infinity(),0}));
    for (int i=0; i<60; ++i) world.step();
    auto s = world.snapshot();
    CHECK(s.tick == 60 && s.players.size() == 2);
    CHECK(test::near(s.players[0].position.x, 5/std::sqrt(2.0)));
    CHECK(test::near(std::hypot(s.players[0].velocity.x,s.players[0].velocity.z),5));
    CHECK(s.players[1].position.x == 1.5);
    CHECK(s.players[0].position.y == 0.5);
    for (int i=0; i<65; ++i) world.step();
    CHECK(world.snapshot().players[0].velocity.x == 0);
    CHECK(world.apply({1,2,0,0})); world.step();
    CHECK(world.snapshot().players[0].velocity == sandbox::Vector3{});
    world.removePlayer(2); CHECK(world.snapshot().players.size() == 1);
    for (std::uint32_t id=2; id<=32; ++id) CHECK(world.addPlayer(id));
    CHECK(!world.addPlayer(33));
}); }
