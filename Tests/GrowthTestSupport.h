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
    if (direction.Length() > 1) {
        direction = direction.Normalized();
    }
    return {id, state.tick + 1, direction.x, direction.z};
}

inline Vector3 nearest_food(const WorldSnapshot& state, std::uint32_t id)
{
    const auto& player = *std::ranges::find(state.players, id, &PlayerState::id);
    check(!state.collectibles.empty(), "Food remains available");
    const auto food = std::ranges::min_element(state.collectibles, {}, [&](const auto& item) {
        return (item.position - player.position).Length();
    });
    return food->position;
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
