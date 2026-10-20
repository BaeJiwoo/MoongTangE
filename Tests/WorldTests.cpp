#include "Common/Game/GameWorld.h"
#include "TestSupport.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <set>

int main()
{
    return MoongTangE::test::run([] {
        constexpr std::uint64_t WORLD_SEED = 123456789;
        MoongTangE::GameWorld seededA(WORLD_SEED), seededB(WORLD_SEED), seededC(WORLD_SEED + 1);
        const auto initialA = seededA.Snapshot();
        const auto initialB = seededB.Snapshot();
        const auto initialC = seededC.Snapshot();
        MOONGTANGE_CHECK(initialA.collectibles.size() ==
                         MoongTangE::simulation::INITIAL_COLLECTIBLES);
        MOONGTANGE_CHECK(initialA.collectibles.size() == initialB.collectibles.size());
        MOONGTANGE_CHECK(initialA.collectibles[0].position == initialB.collectibles[0].position);
        MOONGTANGE_CHECK(initialA.collectibles[0].kind == initialB.collectibles[0].kind);
        MOONGTANGE_CHECK(initialA.collectibles[0].scale == initialB.collectibles[0].scale);
        MOONGTANGE_CHECK(initialA.collectibles[0].position != initialC.collectibles[0].position);

        std::set<double> scales;
        for (std::size_t index = 0; index < initialA.collectibles.size(); ++index) {
            const auto& collectible = initialA.collectibles[index];
            MOONGTANGE_CHECK(collectible.id == index + 1);
            MOONGTANGE_CHECK(MoongTangE::valid_collectible_kind(collectible.kind));
            MOONGTANGE_CHECK(scales.insert(collectible.scale).second);
            MOONGTANGE_CHECK(MoongTangE::test::approx(
                collectible.position.y,
                MoongTangE::simulation::COLLECTIBLE_BASE_RADIUS * collectible.scale));

            for (std::size_t other = 0; other < index; ++other) {
                const auto& previous = initialA.collectibles[other];
                const MoongTangE::Vector3 offset{collectible.position.x - previous.position.x, 0,
                                                 collectible.position.z - previous.position.z};
                const double requiredDistance = MoongTangE::simulation::COLLECTIBLE_BASE_RADIUS *
                                                    (collectible.scale + previous.scale) +
                                                MoongTangE::simulation::COLLECTIBLE_MIN_GAP;
                MOONGTANGE_CHECK(offset.Length() >= requiredDistance);
            }
        }

        for (std::uint64_t tick = 0;
             tick < MoongTangE::simulation::COLLECTIBLE_SPAWN_INTERVAL_TICKS; ++tick) {
            seededA.Step();
            seededB.Step();
        }
        const auto droppedA = seededA.Snapshot();
        const auto droppedB = seededB.Snapshot();
        MOONGTANGE_CHECK(droppedA.collectibles.size() == initialA.collectibles.size() + 1);
        MOONGTANGE_CHECK(droppedA.collectibles.back().position ==
                         droppedB.collectibles.back().position);
        MOONGTANGE_CHECK(droppedA.collectibles.back().position.y ==
                         MoongTangE::simulation::COLLECTIBLE_DROP_HEIGHT);

        while (seededA.CollectibleCount() < MoongTangE::simulation::MAX_COLLECTIBLES) {
            for (std::uint64_t tick = 0;
                 tick < MoongTangE::simulation::COLLECTIBLE_SPAWN_INTERVAL_TICKS; ++tick) {
                seededA.Step();
            }
        }
        for (std::uint64_t tick = 0;
             tick < MoongTangE::simulation::COLLECTIBLE_SPAWN_INTERVAL_TICKS; ++tick) {
            seededA.Step();
        }
        MOONGTANGE_CHECK(seededA.CollectibleCount() == MoongTangE::simulation::MAX_COLLECTIBLES);

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
    });
}
