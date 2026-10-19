#include "Common/Game/GameWorld.h"
#include "TestSupport.h"

#include <cmath>
#include <cstdint>
#include <limits>

int main()
{
    return MoongTangE::test::run([] {
        MoongTangE::GameWorld world;
        MOONGTANGE_CHECK(world.AddPlayer(1));
        MOONGTANGE_CHECK(world.AddPlayer(2));
        MOONGTANGE_CHECK(!world.AddPlayer(1));
        MOONGTANGE_CHECK(!world.AddPlayer(0));

        MOONGTANGE_CHECK(world.Apply({1, 1, 1, 1}));
        MOONGTANGE_CHECK(!world.Apply({1, 1, -1, -1}));
        MOONGTANGE_CHECK(!world.Apply({3, 1, 1, 0}));
        MOONGTANGE_CHECK(!world.Apply({1, 2, 2, 0}));
        MOONGTANGE_CHECK(!world.Apply({1, 2, std::numeric_limits<double>::infinity(), 0}));

        for (int i = 0; i < 20; ++i) {
            world.Step();
        }
        MOONGTANGE_CHECK(world.Snapshot().players[0].position.x ==
                         0); // No steering before ground contact.

        for (int i = 20; i < 60; ++i) {
            world.Step();
        }
        auto s = world.Snapshot();
        MOONGTANGE_CHECK(s.tick == 60 && s.players.size() == 2);
        MOONGTANGE_CHECK(s.players[0].position.x > 0 && s.players[0].velocity.x > 0);
        MOONGTANGE_CHECK(
            MoongTangE::test::approx(s.players[0].velocity.x, s.players[0].velocity.z));

        MoongTangE::GameWorld straight;
        MOONGTANGE_CHECK(straight.AddPlayer(1));
        MOONGTANGE_CHECK(straight.Apply({1, 1, 1, 0}));
        for (int i = 0; i < 60; ++i) {
            straight.Step();
        }
        MOONGTANGE_CHECK(MoongTangE::test::approx(
            s.players[0].velocity.Length(), straight.Snapshot().players[0].velocity.Length()));
        MOONGTANGE_CHECK(MoongTangE::test::approx(s.players[0].angularVelocity.z,
                                                  -s.players[0].velocity.x / 0.5));
        MOONGTANGE_CHECK(s.players[0].orientation != MoongTangE::Quaternion{});
        MOONGTANGE_CHECK(s.players[1].position.x == 1.5);
        MOONGTANGE_CHECK(s.players[0].position.y == 0.5);

        for (int i = 60; i < 120; ++i) {
            world.Step();
        }
        const double speed = world.Snapshot().players[0].velocity.Length();
        world.Step();
        MOONGTANGE_CHECK(world.Snapshot().players[0].velocity.Length() > 0);
        MOONGTANGE_CHECK(world.Snapshot().players[0].velocity.Length() <
                         speed); // Expiry stops force, not motion.

        for (int i = 0; i < 600; ++i) {
            world.Step();
        }
        MOONGTANGE_CHECK(world.Snapshot().players[0].velocity.x == 0);

        MOONGTANGE_CHECK(world.Apply({1, 2, 0, 0}));
        world.Step();
        MOONGTANGE_CHECK(world.Snapshot().players[0].velocity == MoongTangE::Vector3{});

        world.RemovePlayer(2);
        MOONGTANGE_CHECK(world.Snapshot().players.size() == 1);

        for (std::uint32_t id = 2; id <= 32; ++id) {
            MOONGTANGE_CHECK(world.AddPlayer(id));
        }
        MOONGTANGE_CHECK(!world.AddPlayer(33));

        MoongTangE::GameWorld collisionWorld;
        MOONGTANGE_CHECK(collisionWorld.AddPlayer(1));
        MOONGTANGE_CHECK(collisionWorld.AddPlayer(2));
        MOONGTANGE_CHECK(collisionWorld.Apply({1, 1, 1, 0}));
        MOONGTANGE_CHECK(collisionWorld.Apply({2, 1, -1, 0}));
        for (int tick = 0; tick < 180; ++tick) {
            collisionWorld.Step();
            const auto players = collisionWorld.Snapshot().players;
            const auto separation = players[1].position - players[0].position;
            MOONGTANGE_CHECK(separation.Length() + 1e-9 >=
                             2 * MoongTangE::simulation::PLAYER_RADIUS);
            MOONGTANGE_CHECK(players[0].position.x <= players[1].position.x);
        }
    });
}
