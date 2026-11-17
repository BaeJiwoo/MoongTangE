#include "GrowthTestSupport.h"

#include <limits>

using namespace MoongTangE;

int main()
{
    return test::run([] {
        GameWorld world(123456789), replay(123456789), different(123456790);
        MOONGTANGE_CHECK(world.Snapshot().collectibles.size() == simulation::INITIAL_COLLECTIBLES);
        MOONGTANGE_CHECK(world.Snapshot().collectibles.front().position !=
                         different.Snapshot().collectibles.front().position);
        MOONGTANGE_CHECK(!world.AddPlayer(0));
        MOONGTANGE_CHECK(world.AddPlayer(1) && replay.AddPlayer(1));
        MOONGTANGE_CHECK(!world.AddPlayer(1));
        MOONGTANGE_CHECK(!world.Apply({2, 1, 1, 0}));
        MOONGTANGE_CHECK(!world.Apply({1, 1, std::numeric_limits<double>::quiet_NaN(), 0}));
        bool ate = false;
        for (std::uint64_t tick = 0; tick < 1800; ++tick) {
            const auto before = world.Snapshot();
            const auto input = test::steer_to(before, 1, test::nearest_food(before, 1));
            MOONGTANGE_CHECK(world.Apply(input) && replay.Apply(input));
            MOONGTANGE_CHECK(!world.Apply(input));
            world.Step();
            replay.Step();
            const auto a = world.Snapshot(), b = replay.Snapshot();
            MOONGTANGE_CHECK(a.players[0].position == b.players[0].position &&
                             a.players[0].radius == b.players[0].radius &&
                             a.players[0].foodEaten == b.players[0].foodEaten);
            MOONGTANGE_CHECK(a.players[0].position.Finite() && a.players[0].orientation.Unit());
            MOONGTANGE_CHECK(a.players[0].position.y == a.players[0].radius);
            MOONGTANGE_CHECK(a.players[0].radius >= simulation::PLAYER_RADIUS &&
                             a.players[0].radius <= simulation::MAX_PLAYER_RADIUS);
            MOONGTANGE_CHECK(a.collectibles.size() <= MAX_COLLECTIBLES &&
                             a.collectibles.size() == b.collectibles.size());
            for (std::size_t index = 0; index < a.collectibles.size(); ++index) {
                MOONGTANGE_CHECK(a.collectibles[index].id == b.collectibles[index].id);
                MOONGTANGE_CHECK(a.collectibles[index].attachedPlayerId == 0);
            }
            if (a.players[0].foodEaten > before.players[0].foodEaten) {
                ate = true;
                double consumedVolume = 0;
                for (const auto& food : before.collectibles) {
                    if (std::ranges::find(a.collectibles, food.id, &CollectibleState::id) ==
                        a.collectibles.end()) {
                        consumedVolume +=
                            std::pow(simulation::COLLECTIBLE_BASE_RADIUS * food.scale, 3);
                    }
                }
                MOONGTANGE_CHECK(consumedVolume > 0);
                if (a.players[0].mineHitTick == before.players[0].mineHitTick) {
                    MOONGTANGE_CHECK(test::approx(
                        std::pow(a.players[0].radius, 3),
                        std::min(std::pow(simulation::MAX_PLAYER_RADIUS, 3),
                                 std::pow(before.players[0].radius, 3) + consumedVolume)));
                }
            }
        }
        MOONGTANGE_CHECK(ate && world.Snapshot().players[0].foodEaten > 5);
        for (int tick = 0; tick < 240; ++tick) {
            world.Step();
        }
        MOONGTANGE_CHECK(world.Snapshot().players[0].velocity == Vector3{});
        world.RemovePlayer(1);
        MOONGTANGE_CHECK(!world.HasPlayer(1) && world.AddPlayer(2));
        MOONGTANGE_CHECK(world.Snapshot().players[0].radius == simulation::PLAYER_RADIUS);

        GameWorld straight, diagonal;
        MOONGTANGE_CHECK(straight.AddPlayer(1) && diagonal.AddPlayer(1));
        for (std::uint64_t tick = 1; tick <= 10; ++tick) {
            MOONGTANGE_CHECK(straight.Apply({1, tick, 1, 0}));
            MOONGTANGE_CHECK(diagonal.Apply({1, tick, 1, 1}));
            straight.Step();
            diagonal.Step();
        }
        MOONGTANGE_CHECK(test::approx(straight.Snapshot().players[0].velocity.Length(),
                                      diagonal.Snapshot().players[0].velocity.Length()));
        for (std::uint64_t tick = 11; tick < 1500; ++tick) {
            MOONGTANGE_CHECK(straight.Apply({1, tick, 1, 0}));
            straight.Step();
        }
        const auto player = straight.Snapshot().players[0];
        MOONGTANGE_CHECK(player.position.x + player.radius <= simulation::MAP_HALF_EXTENT);
    });
}
