#include "GrowthTestSupport.h"

using namespace MoongTangE;

namespace {

InputCommand aim_directly(const WorldSnapshot& state, std::uint32_t id, Vector3 target)
{
    const auto& player = *std::ranges::find(state.players, id, &PlayerState::id);
    auto direction = target - player.position;
    direction.y = 0;
    direction = direction.Normalized();
    return {id, state.tick + 1, direction.x, direction.z};
}

void large_food_waits_until_the_player_grows()
{
    // Find a reproducible approach without incidental growth from another food on the path.
    for (std::uint64_t seed = 1; seed <= 32; ++seed) {
        GameWorld world(seed);
        MOONGTANGE_CHECK(world.AddPlayer(1));
        const auto initial = world.Snapshot();
        const auto target =
            std::ranges::min_element(initial.collectibles, {}, [](const auto& food) {
                return simulation::COLLECTIBLE_BASE_RADIUS * food.scale > simulation::PLAYER_RADIUS
                           ? food.position.Length()
                           : 1e9;
            });
        const auto food = *target;
        const double foodRadius = simulation::COLLECTIBLE_BASE_RADIUS * food.scale;
        bool reachedWhileSmall = false;
        for (int tick = 0; tick < 1000; ++tick) {
            const auto state = world.Snapshot();
            if (state.players[0].radius >= foodRadius) {
                break;
            }
            if (horizontal_distance(state.players[0].position, food.position) < 0.1) {
                reachedWhileSmall = true;
                break;
            }
            MOONGTANGE_CHECK(world.Apply(test::steer_to(state, 1, food.position)));
            world.Step();
        }
        if (!reachedWhileSmall) {
            continue;
        }
        for (int tick = 0; tick < 120; ++tick) {
            const auto state = world.Snapshot();
            MOONGTANGE_CHECK(std::ranges::find(state.collectibles, food.id,
                                               &CollectibleState::id) != state.collectibles.end());
            MOONGTANGE_CHECK(state.players[0].radius < foodRadius);
            MOONGTANGE_CHECK(world.Apply({1, state.tick + 1, 0, 0}));
            world.Step();
        }
        test::grow_player(world, 1, foodRadius);
        for (int tick = 0; tick < 3000; ++tick) {
            const auto state = world.Snapshot();
            if (std::ranges::find(state.collectibles, food.id, &CollectibleState::id) ==
                state.collectibles.end()) {
                return;
            }
            MOONGTANGE_CHECK(world.Apply(test::steer_to(state, 1, food.position)));
            world.Step();
        }
        MOONGTANGE_CHECK(false); // The same food must become edible after growth.
    }
    MOONGTANGE_CHECK(false); // An undersized player must be able to overlap food without eating it.
}

void small_attacker_cannot_eat_a_large_player()
{
    // Run both ID orderings: attacker is determined by size, never input direction or player ID.
    for (const std::uint32_t largerId : {1U, 2U}) {
        GameWorld world;
        const std::uint32_t smallerId = 3 - largerId;
        MOONGTANGE_CHECK(world.AddPlayer(largerId));
        test::grow_player(world, largerId, 1.4);
        MOONGTANGE_CHECK(world.AddPlayer(smallerId));
        bool met = false;
        for (int tick = 0; tick < 4000; ++tick) {
            const auto state = world.Snapshot();
            const auto& larger = *std::ranges::find(state.players, largerId, &PlayerState::id);
            const auto& smaller = *std::ranges::find(state.players, smallerId, &PlayerState::id);
            MOONGTANGE_CHECK(larger.deathTick == 0);
            if (smaller.deathTick != 0) {
                met = true;
                break;
            }
            MOONGTANGE_CHECK(smaller.radius < larger.radius);
            MOONGTANGE_CHECK(world.Apply(test::steer_to(state, smallerId, larger.position)));
            MOONGTANGE_CHECK(world.Apply({largerId, state.tick + 1, 0, 0}));
            world.Step();
        }
        MOONGTANGE_CHECK(met);
    }
}

void rocks_block_and_mud_slows_movement()
{
    GameWorld world(24);
    MOONGTANGE_CHECK(world.AddPlayer(1));
    const auto rock = ARENA_ROCKS.front();
    bool touched = false;
    for (int tick = 0; tick < 700; ++tick) {
        const auto before = world.Snapshot();
        MOONGTANGE_CHECK(world.Apply(aim_directly(before, 1, rock.position)));
        world.Step();
        const auto player = world.Snapshot().players[0];
        const double separation = horizontal_distance(player.position, rock.position);
        MOONGTANGE_CHECK(separation >= player.radius + rock.radius - 1e-8);
        touched = touched || separation <= player.radius + rock.radius + 0.05;
    }
    MOONGTANGE_CHECK(touched);

    GameWorld mudWorld(24);
    MOONGTANGE_CHECK(mudWorld.AddPlayer(1));
    int mudTicks = 0;
    bool checkedMud = false, checkedRecovery = false;
    for (int tick = 0; tick < 1000; ++tick) {
        const auto before = mudWorld.Snapshot();
        MOONGTANGE_CHECK(mudWorld.Apply({1, before.tick + 1, 0, -1}));
        mudWorld.Step();
        const auto player = mudWorld.Snapshot().players[0];
        const double baseSpeed = simulation::PLAYER_MOVE_SPEED /
                                 std::pow(player.radius / simulation::PLAYER_RADIUS, 0.25);
        mudTicks = in_mud(player.position) ? mudTicks + 1 : 0;
        if (mudTicks > 30) {
            MOONGTANGE_CHECK(player.velocity.Length() < baseSpeed * 0.5);
            checkedMud = true;
        }
        if (checkedMud && player.position.z < -23 && !in_mud(player.position)) {
            MOONGTANGE_CHECK(player.velocity.Length() > baseSpeed * 0.9);
            checkedRecovery = true;
            break;
        }
    }
    MOONGTANGE_CHECK(checkedMud && checkedRecovery);
}

void expanded_food_spawns_clear_of_rocks()
{
    GameWorld world(42);
    auto verify = [](const WorldSnapshot& state) {
        for (const auto& food : state.collectibles) {
            const double radius = simulation::COLLECTIBLE_BASE_RADIUS * food.scale;
            MOONGTANGE_CHECK(!overlaps_rock(food.position, radius));
            MOONGTANGE_CHECK(std::abs(food.position.x) + radius < simulation::MAP_HALF_EXTENT);
            MOONGTANGE_CHECK(std::abs(food.position.z) + radius < simulation::MAP_HALF_EXTENT);
        }
    };
    verify(world.Snapshot());
    MOONGTANGE_CHECK(std::ranges::any_of(world.Snapshot().collectibles, [](const auto& food) {
        return std::abs(food.position.x) > 32 || std::abs(food.position.z) > 32;
    }));
    for (int tick = 0; tick < 1200; ++tick) {
        world.Step();
    }
    verify(world.Snapshot());
    MOONGTANGE_CHECK(world.CollectibleCount() == simulation::MAX_COLLECTIBLES);
}

} // namespace

int main()
{
    return test::run([] {
        large_food_waits_until_the_player_grows();
        small_attacker_cannot_eat_a_large_player();
        rocks_block_and_mud_slows_movement();
        expanded_food_spawns_clear_of_rocks();
    });
}
