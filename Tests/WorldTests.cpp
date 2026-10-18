#include "Common/Game/GameWorld.h"
#include "TestSupport.h"
#include <cstdint>
#include <limits>

int main() {
    return MoongTangE::test::run([] {
        MoongTangE::GameWorld world;
        MOONGTANGE_CHECK(world.addPlayer(1));
        MOONGTANGE_CHECK(world.addPlayer(2));
        MOONGTANGE_CHECK(!world.addPlayer(1));
        MOONGTANGE_CHECK(!world.addPlayer(0));
        MOONGTANGE_CHECK(world.apply({1, 1, 1, 1}));
        MOONGTANGE_CHECK(!world.apply({1, 1, -1, -1}));
        MOONGTANGE_CHECK(!world.apply({3, 1, 1, 0}));
        MOONGTANGE_CHECK(!world.apply({1, 2, 2, 0}));
        MOONGTANGE_CHECK(!world.apply({1, 2, std::numeric_limits<double>::infinity(), 0}));
        for (int i = 0; i < 20; ++i) {
            world.step();
        }
        MOONGTANGE_CHECK(world.snapshot().players[0].position.x ==
                         0); // No steering before ground contact.
        for (int i = 20; i < 60; ++i) {
            world.step();
        }
        auto s = world.snapshot();
        MOONGTANGE_CHECK(s.tick == 60 && s.players.size() == 2);
        MOONGTANGE_CHECK(s.players[0].position.x > 0 && s.players[0].velocity.x > 0);
        MOONGTANGE_CHECK(
            MoongTangE::test::approx(s.players[0].velocity.x, s.players[0].velocity.z));
        MoongTangE::GameWorld straight;
        MOONGTANGE_CHECK(straight.addPlayer(1));
        MOONGTANGE_CHECK(straight.apply({1, 1, 1, 0}));
        for (int i = 0; i < 60; ++i) {
            straight.step();
        }
        MOONGTANGE_CHECK(MoongTangE::test::approx(
            s.players[0].velocity.length(), straight.snapshot().players[0].velocity.length()));
        MOONGTANGE_CHECK(MoongTangE::test::approx(s.players[0].angularVelocity.z,
                                                  -s.players[0].velocity.x / 0.5));
        MOONGTANGE_CHECK(s.players[0].orientation != MoongTangE::Quaternion{});
        MOONGTANGE_CHECK(s.players[1].position.x == 1.5);
        MOONGTANGE_CHECK(s.players[0].position.y == 0.5);
        for (int i = 60; i < 120; ++i) {
            world.step();
        }
        const double speed = world.snapshot().players[0].velocity.length();
        world.step();
        MOONGTANGE_CHECK(world.snapshot().players[0].velocity.length() > 0);
        MOONGTANGE_CHECK(world.snapshot().players[0].velocity.length() <
                         speed); // Expiry stops force, not motion.
        for (int i = 0; i < 600; ++i) {
            world.step();
        }
        MOONGTANGE_CHECK(world.snapshot().players[0].velocity.x == 0);
        MOONGTANGE_CHECK(world.apply({1, 2, 0, 0}));
        world.step();
        MOONGTANGE_CHECK(world.snapshot().players[0].velocity == MoongTangE::Vector3{});
        world.removePlayer(2);
        MOONGTANGE_CHECK(world.snapshot().players.size() == 1);
        for (std::uint32_t id = 2; id <= 32; ++id) {
            MOONGTANGE_CHECK(world.addPlayer(id));
        }
        MOONGTANGE_CHECK(!world.addPlayer(33));
    });
}
