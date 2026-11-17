#pragma once

#include "Common/Protocol/WorldSnapshot.h"

#include <algorithm>
#include <vector>

namespace MoongTangE {

inline double player_mass(const PlayerState& player)
{
    const double ratio = player.radius / simulation::PLAYER_RADIUS;
    return simulation::PLAYER_MASS * ratio * ratio * ratio;
}

inline std::vector<const PlayerState*> ranked_players(const WorldSnapshot& state)
{
    std::vector<const PlayerState*> players;
    for (const auto& player : state.players) {
        if (player.deathTick == 0) {
            players.push_back(&player);
        }
    }
    std::ranges::sort(players, [](const auto* first, const auto* second) {
        return first->radius != second->radius ? first->radius > second->radius
                                               : first->id < second->id;
    });
    return players;
}

} // namespace MoongTangE
