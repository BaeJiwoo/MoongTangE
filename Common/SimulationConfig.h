#pragma once
#include <cstddef>
#include <cstdint>

namespace MoongTangE::simulation {
inline constexpr std::uint32_t TICKS_PER_SECOND = 60;
inline constexpr double FIXED_DELTA = 1.0 / TICKS_PER_SECOND;
inline constexpr std::uint64_t SNAPSHOT_INTERVAL_TICKS = 3;
inline constexpr std::uint64_t INPUT_TIMEOUT_TICKS = 2 * TICKS_PER_SECOND;
inline constexpr std::size_t MAX_PLAYERS = 32;
inline constexpr double GRAVITY = 9.81;
inline constexpr double PLAYER_RADIUS = 0.5;
inline constexpr double PLAYER_MASS = 1.0;
inline constexpr double SPAWN_HEIGHT = 2.0;
inline constexpr double SPAWN_SPACING = 1.5;
inline constexpr std::uint32_t SPAWN_COLUMNS = 8;
inline constexpr double MOVEMENT_FORCE = 10.0;

// Player IDs are assigned by the server and are nonzero.
inline constexpr double spawnX(std::uint32_t playerId) {
    return static_cast<double>((playerId - 1) % SPAWN_COLUMNS) * SPAWN_SPACING;
}
} // namespace MoongTangE::simulation