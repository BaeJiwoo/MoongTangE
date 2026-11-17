#pragma once

#include "Common/Math/Vector3.h"

#include <array>
#include <cmath>

namespace MoongTangE {

struct ArenaZone
{
    Vector3 position;
    double radius{};
};

// Immutable, protocol-versioned arena geometry shared by simulation and rendering.
inline constexpr std::array<ArenaZone, 10> ARENA_ROCKS{{{{-12, 0, -15}, 2.2},
                                                        {{12, 0, -15}, 2.2},
                                                        {{-13, 0, 16}, 2.5},
                                                        {{13, 0, 20}, 2.5},
                                                        {{-29, 0, -22}, 3.0},
                                                        {{29, 0, -22}, 3.0},
                                                        {{-29, 0, 23}, 3.0},
                                                        {{29, 0, 25}, 3.0},
                                                        {{-6, 0, -34}, 2.8},
                                                        {{6, 0, 35}, 2.8}}};
inline constexpr std::array<ArenaZone, 6> ARENA_MUD{{{{0, 0, -17}, 4.0},
                                                     {{-21, 0, 9}, 4.5},
                                                     {{22, 0, 5}, 4.5},
                                                     {{0, 0, 24}, 4.0},
                                                     {{-22, 0, -34}, 4.5},
                                                     {{27, 0, 37}, 4.0}}};
inline constexpr double MUD_SPEED_FACTOR = 0.45;

inline double horizontal_distance(Vector3 first, Vector3 second)
{
    return std::hypot(first.x - second.x, first.z - second.z);
}

inline bool overlaps_rock(Vector3 position, double radius)
{
    for (const auto& rock : ARENA_ROCKS) {
        if (horizontal_distance(position, rock.position) < radius + rock.radius) {
            return true;
        }
    }
    return false;
}

inline bool in_mud(Vector3 position)
{
    for (const auto& mud : ARENA_MUD) {
        if (horizontal_distance(position, mud.position) < mud.radius) {
            return true;
        }
    }
    return false;
}

} // namespace MoongTangE
