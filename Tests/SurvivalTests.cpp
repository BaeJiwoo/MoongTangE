#include "Common/Game/GameWorld.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

using namespace MoongTangE;

void chase(GameWorld& world, std::uint32_t hunterId, std::uint32_t targetId,
           std::uint64_t restartDeathTick = 0)
{
    const auto state = world.Snapshot();
    const auto& hunter = state.players[hunterId - 1];
    const auto& target = state.players[targetId - 1];
    auto direction = target.position - hunter.position;
    direction.y = 0;
    direction = direction.Normalized();
    MOONGTANGE_CHECK(
        world.Apply({hunterId, state.tick + 1, direction.x, direction.z, restartDeathTick}));
}

void smaller_ball_dies_and_can_restart_without_rejoining()
{
    // Seed 24 puts one collectible at player 1's landing area, away from player 2.
    GameWorld world(24);
    MOONGTANGE_CHECK(world.AddPlayer(1) && world.AddPlayer(2));
    for (int tick = 0; tick < 90; ++tick) {
        world.Step();
    }
    auto state = world.Snapshot();
    MOONGTANGE_CHECK(std::any_of(state.collectibles.begin(), state.collectibles.end(),
                                 [](const auto& collectible) {
                                     return collectible.attachedPlayerId == 1;
                                 }));
    MOONGTANGE_CHECK(std::none_of(state.collectibles.begin(), state.collectibles.end(),
                                  [](const auto& collectible) {
                                      return collectible.attachedPlayerId == 2;
                                  }));
    MOONGTANGE_CHECK(state.players[0].deathTick == 0 && state.players[1].deathTick == 0);

    for (int tick = 0; tick < 200 && state.players[1].deathTick == 0; ++tick) {
        chase(world, 2, 1);
        world.Step();
        state = world.Snapshot();
    }
    const auto deathTick = state.players[1].deathTick;
    const auto deathPosition = state.players[1].position;
    MOONGTANGE_CHECK(deathTick != 0 && state.players[0].deathTick == 0);
    MOONGTANGE_CHECK(world.PlayerCount() == 2 && world.HasPlayer(2));

    // Movement, wrong defeat IDs, and a stale input cannot revive or move a defeated player.
    for (int tick = 0; tick < 120; ++tick) {
        MOONGTANGE_CHECK(world.Apply({2, state.tick + 1, 1, 1, deathTick + 1}));
        MOONGTANGE_CHECK(!world.Apply({2, state.tick + 1, 0, 0, deathTick}));
        world.Step();
        state = world.Snapshot();
        MOONGTANGE_CHECK(state.players[1].position == deathPosition);
        MOONGTANGE_CHECK(state.players[1].deathTick == deathTick);
        MOONGTANGE_CHECK(state.players[1].velocity == Vector3{});
        MOONGTANGE_CHECK(std::none_of(state.collectibles.begin(), state.collectibles.end(),
                                      [](const auto& collectible) {
                                          return collectible.attachedPlayerId == 2;
                                      }));
    }

    MOONGTANGE_CHECK(world.Apply({2, state.tick + 1, 0, 0, deathTick}));
    world.Step();
    state = world.Snapshot();
    MOONGTANGE_CHECK(state.players[1].deathTick == 0);
    MOONGTANGE_CHECK((state.players[1].position - state.players[0].position).Length() > 3);
    MOONGTANGE_CHECK(state.players[1].orientation == Quaternion{});
    MOONGTANGE_CHECK(std::abs(state.players[1].position.x) <
                     simulation::COLLECTIBLE_AREA_HALF_EXTENT);
    MOONGTANGE_CHECK(std::abs(state.players[1].position.z) <
                     simulation::COLLECTIBLE_AREA_HALF_EXTENT);

    // Repeated delivery of the old request must not revive a subsequent defeat.
    for (int tick = 0; tick < 2400 && state.players[1].deathTick == 0; ++tick) {
        chase(world, 2, 1, deathTick);
        world.Step();
        state = world.Snapshot();
    }
    MOONGTANGE_CHECK(state.players[1].deathTick > deathTick);
    const auto nextDeathTick = state.players[1].deathTick;
    for (int tick = 0; tick < 6; ++tick) {
        MOONGTANGE_CHECK(world.Apply({2, state.tick + 1, 0, 0, deathTick}));
        world.Step();
        state = world.Snapshot();
        MOONGTANGE_CHECK(state.players[1].deathTick == nextDeathTick);
    }
    MOONGTANGE_CHECK(world.Apply({2, state.tick + 1, 0, 0, nextDeathTick}));
    world.Step();
    MOONGTANGE_CHECK(world.Snapshot().players[1].deathTick == 0);
    world.RemovePlayer(2);
    MOONGTANGE_CHECK(!world.HasPlayer(2) && world.AddPlayer(3));
}

void larger_ball_wins_regardless_of_player_id_order()
{
    GameWorld world(24);
    MOONGTANGE_CHECK(world.AddPlayer(2) && world.AddPlayer(1));
    for (int tick = 0; tick < 90; ++tick) {
        world.Step();
    }
    auto state = world.Snapshot();
    for (int tick = 0; tick < 200 && state.players[0].deathTick == 0; ++tick) {
        chase(world, 1, 2);
        world.Step();
        state = world.Snapshot();
    }
    MOONGTANGE_CHECK(state.players[0].deathTick != 0 && state.players[1].deathTick == 0);
}

void expanded_spawns_cover_the_new_area_and_scale_range()
{
    GameWorld world;
    while (world.CollectibleCount() < simulation::MAX_COLLECTIBLES) {
        world.Step();
    }
    for (int tick = 0; tick < 180; ++tick) {
        world.Step();
    }
    const auto state = world.Snapshot();
    double smallest = simulation::COLLECTIBLE_MAX_SCALE;
    double largest = simulation::COLLECTIBLE_MIN_SCALE;
    bool outsideOldArea = false;
    for (const auto& collectible : state.collectibles) {
        smallest = std::min(smallest, collectible.scale);
        largest = std::max(largest, collectible.scale);
        outsideOldArea = outsideOldArea || std::abs(collectible.position.x) > 12 ||
                         std::abs(collectible.position.z) > 12;
        MOONGTANGE_CHECK(collectible.position.Finite() && collectible.orientation.Unit());
        MOONGTANGE_CHECK(std::abs(collectible.position.x) <=
                         simulation::COLLECTIBLE_AREA_HALF_EXTENT);
        MOONGTANGE_CHECK(std::abs(collectible.position.z) <=
                         simulation::COLLECTIBLE_AREA_HALF_EXTENT);
    }
    MOONGTANGE_CHECK(outsideOldArea);
    MOONGTANGE_CHECK(test::approx(smallest, 0.4) && test::approx(largest, 2.0));
}

void equal_balls_collide_without_defeat()
{
    GameWorld world(123456789);
    MOONGTANGE_CHECK(world.AddPlayer(1) && world.AddPlayer(2));
    for (int tick = 0; tick < 180; ++tick) {
        MOONGTANGE_CHECK(world.Apply({1, static_cast<std::uint64_t>(tick + 1), 1, 0}));
        MOONGTANGE_CHECK(world.Apply({2, static_cast<std::uint64_t>(tick + 1), -1, 0}));
        world.Step();
        const auto state = world.Snapshot();
        MOONGTANGE_CHECK(state.players[0].deathTick == 0 && state.players[1].deathTick == 0);
        MOONGTANGE_CHECK(state.players[0].position.x <= state.players[1].position.x);
        MOONGTANGE_CHECK((state.players[0].position - state.players[1].position).Length() >=
                         2 * simulation::PLAYER_RADIUS - 1e-9);
    }
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        smaller_ball_dies_and_can_restart_without_rejoining();
        larger_ball_wins_regardless_of_player_id_order();
        expanded_spawns_cover_the_new_area_and_scale_range();
        equal_balls_collide_without_defeat();
    });
}
