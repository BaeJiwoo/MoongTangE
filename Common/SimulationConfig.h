#pragma once

#include <cstddef>
#include <cstdint>

namespace MoongTangE::simulation {

inline constexpr std::uint32_t TICKS_PER_SECOND = 60;
inline constexpr double FIXED_DELTA = 1.0 / TICKS_PER_SECOND;
inline constexpr std::uint64_t SNAPSHOT_INTERVAL_TICKS = 3;
inline constexpr std::uint64_t INPUT_TIMEOUT_TICKS = 2 * TICKS_PER_SECOND;

inline constexpr std::uint64_t ROUND_COUNTDOWN_TICKS = 3 * TICKS_PER_SECOND;
inline constexpr std::uint64_t ROUND_DURATION_TICKS = 90 * TICKS_PER_SECOND;
inline constexpr std::uint64_t ROUND_RESULTS_TICKS = 3 * TICKS_PER_SECOND;
inline constexpr std::uint32_t COLLECTION_POINTS = 100;
inline constexpr std::uint32_t SOLO_TARGET_SCORE = 20 * COLLECTION_POINTS;

inline constexpr std::size_t MAX_PLAYERS = 32;

inline constexpr double GRAVITY = 9.81;
inline constexpr double PLAYER_RADIUS = 0.5;
inline constexpr double PLAYER_MASS = 1.0;
inline constexpr double MAX_PLAYER_RADIUS = 3.0;
inline constexpr double PLAYER_MOVE_SPEED = 6.0;
inline constexpr double PLAYER_RESPONSE = 18.0;
inline constexpr double EAT_VOLUME_RATIO = 1.15;
inline constexpr std::uint32_t MAX_FOOD_PER_ROUND = 4096;
inline constexpr std::size_t MAX_MINES = 6;
inline constexpr double MINE_RADIUS = 0.65;
inline constexpr double MINE_TRIGGER_RADIUS = 0.9;
inline constexpr double MINE_RETAINED_VOLUME = 0.6;
inline constexpr std::uint64_t MINE_RESPAWN_TICKS = 5 * TICKS_PER_SECOND;
inline constexpr std::uint64_t MINE_IMMUNITY_TICKS = 2 * TICKS_PER_SECOND;

inline constexpr double SPAWN_HEIGHT = 2.0;
inline constexpr double SPAWN_SPACING = 1.5;
inline constexpr std::uint32_t SPAWN_COLUMNS = 8;

inline constexpr double MOVEMENT_FORCE = 10.0;
inline constexpr double ATTACHED_MOVEMENT_MASS_EXPONENT = 1.0;
inline constexpr double ATTACHED_MOVEMENT_FORCE_LIMIT = 10.0;

inline constexpr std::uint64_t DEFAULT_WORLD_SEED = 0x4d6f6f6e6754616eULL;
inline constexpr std::size_t INITIAL_COLLECTIBLES = 64;
inline constexpr std::size_t MAX_COLLECTIBLES = 96;
inline constexpr std::uint64_t COLLECTIBLE_SPAWN_INTERVAL_TICKS = TICKS_PER_SECOND / 2;
inline constexpr std::size_t COLLECTIBLE_SPAWN_ATTEMPTS = 256;
inline constexpr double MAP_HALF_EXTENT = 32.0;
inline constexpr int MAP_GRID_SLICES = 64;
inline constexpr double COLLECTIBLE_AREA_HALF_EXTENT = 24.0;
inline constexpr double COLLECTIBLE_DROP_HEIGHT = 8.0;
inline constexpr double COLLECTIBLE_BASE_RADIUS = 0.35;
inline constexpr double COLLECTIBLE_BASE_MASS = 0.25;
inline constexpr double COLLECTIBLE_MIN_SCALE = 0.4;
inline constexpr double COLLECTIBLE_MAX_SCALE = 2.0;
inline constexpr double COLLECTIBLE_MIN_GAP = 0.75;
inline constexpr double COLLECTIBLE_EMBED_RATIO = 0.25;
inline constexpr std::uint32_t ATTACHMENT_SETTLE_TICKS = 9;

// Player IDs are assigned by the server and are nonzero.
inline constexpr double spawn_x(std::uint32_t playerId)
{
    return static_cast<double>((playerId - 1) % SPAWN_COLUMNS) * SPAWN_SPACING;
}

} // namespace MoongTangE::simulation
