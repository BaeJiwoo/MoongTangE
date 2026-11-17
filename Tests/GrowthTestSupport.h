#pragma once

#include "Common/Game/GameWorld.h"
#include "Tests/TestSupport.h"

#include <algorithm>

namespace MoongTangE::test {

inline InputCommand steer_to(const WorldSnapshot& state, std::uint32_t id, Vector3 destination)
{
    const auto& player = *std::ranges::find(state.players, id, &PlayerState::id);
    auto direction = destination - player.position;
    direction.y = 0;
    // Test bots route around static rocks instead of endlessly pushing into them.
    const double travel = direction.Length();
    if (travel > 0) {
        const auto forward = direction.Normalized();
        const Vector3 side{-forward.z, 0, forward.x};
        double nearest = travel;
        for (const auto& rock : ARENA_ROCKS) {
            auto offset = rock.position - player.position;
            offset.y = 0;
            const double along = dot(offset, forward);
            const double clearance = rock.radius + player.radius + 0.35;
            if (along <= 0 || along >= nearest || std::abs(dot(offset, side)) >= clearance) {
                continue;
            }
            nearest = along;
            const double sign = dot(offset, side) > 0 ? -1.0 : 1.0;
            direction = rock.position + side * (sign * (clearance + 1)) - player.position;
            direction.y = 0;
        }
    }
    if (direction.Length() > 1) {
        direction = direction.Normalized();
    }
    return {id, state.tick + 1, direction.x, direction.z};
}

inline Vector3 nearest_food(const WorldSnapshot& state, std::uint32_t id)
{
    const auto& player = *std::ranges::find(state.players, id, &PlayerState::id);
    check(!state.collectibles.empty(), "Food remains available");
    const CollectibleState* nearest = nullptr;
    double distance = 1e9;
    for (const auto& food : state.collectibles) {
        if (simulation::COLLECTIBLE_BASE_RADIUS * food.scale > player.radius) {
            continue;
        }
        const double candidate = (food.position - player.position).Length();
        if (candidate < distance) {
            nearest = &food;
            distance = candidate;
        }
    }
    check(nearest != nullptr, "Edible food remains available");
    return nearest->position;
}

inline void grow_player(GameWorld& world, std::uint32_t id, double radius)
{
    for (int tick = 0; tick < 10000; ++tick) {
        const auto state = world.Snapshot();
        if (std::ranges::find(state.players, id, &PlayerState::id)->radius >= radius) {
            return;
        }
        check(world.Apply(steer_to(state, id, nearest_food(state, id))), "Growth input accepted");
        world.Step();
    }
    check(false, "Player reached target size");
}

} // namespace MoongTangE::test
