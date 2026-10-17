#include "Common/Game/GameWorld.h"
#include "TestSupport.h"
#include <cstdint>
#include <limits>

int main() {
    return test::run([] {
        sandbox::GameWorld world;
        CHECK(world.addPlayer(1));
        CHECK(world.addPlayer(2));
        CHECK(!world.addPlayer(1));
        CHECK(!world.addPlayer(0));
        CHECK(world.apply({1, 1, 1, 1}));
        CHECK(!world.apply({1, 1, -1, -1}));
        CHECK(!world.apply({3, 1, 1, 0}));
        CHECK(!world.apply({1, 2, 2, 0}));
        CHECK(!world.apply({1, 2, std::numeric_limits<double>::infinity(), 0}));
        for (int i = 0; i < 20; ++i) {
            world.step();
        }
        CHECK(world.snapshot().players[0].position.x == 0); // No steering before ground contact.
        for (int i = 20; i < 60; ++i) {
            world.step();
        }
        auto s = world.snapshot();
        CHECK(s.tick == 60 && s.players.size() == 2);
        CHECK(s.players[0].position.x > 0 && s.players[0].velocity.x > 0);
        CHECK(test::approx(s.players[0].velocity.x, s.players[0].velocity.z));
        sandbox::GameWorld straight;
        CHECK(straight.addPlayer(1));
        CHECK(straight.apply({1, 1, 1, 0}));
        for (int i = 0; i < 60; ++i) {
            straight.step();
        }
        CHECK(test::approx(s.players[0].velocity.length(),
                           straight.snapshot().players[0].velocity.length()));
        CHECK(test::approx(s.players[0].angularVelocity.z, -s.players[0].velocity.x / 0.5));
        CHECK(s.players[0].orientation != sandbox::Quaternion{});
        CHECK(s.players[1].position.x == 1.5);
        CHECK(s.players[0].position.y == 0.5);
        for (int i = 60; i < 120; ++i) {
            world.step();
        }
        const double speed = world.snapshot().players[0].velocity.length();
        world.step();
        CHECK(world.snapshot().players[0].velocity.length() > 0);
        CHECK(world.snapshot().players[0].velocity.length() <
              speed); // Expiry stops force, not motion.
        for (int i = 0; i < 600; ++i) {
            world.step();
        }
        CHECK(world.snapshot().players[0].velocity.x == 0);
        CHECK(world.apply({1, 2, 0, 0}));
        world.step();
        CHECK(world.snapshot().players[0].velocity == sandbox::Vector3{});
        world.removePlayer(2);
        CHECK(world.snapshot().players.size() == 1);
        for (std::uint32_t id = 2; id <= 32; ++id) {
            CHECK(world.addPlayer(id));
        }
        CHECK(!world.addPlayer(33));
    });
}
