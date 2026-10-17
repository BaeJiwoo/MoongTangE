#pragma once
#include <cstddef>
#include <cstdint>

namespace sandbox::simulation {
inline constexpr std::uint32_t ticksPerSecond = 60;
inline constexpr double fixedDelta = 1.0 / ticksPerSecond;
inline constexpr std::uint64_t snapshotIntervalTicks = 3;
inline constexpr std::uint64_t inputTimeoutTicks = 2 * ticksPerSecond;
inline constexpr std::size_t maxPlayers = 32;
inline constexpr double gravity = 9.81;
inline constexpr double playerRadius = 0.5;
inline constexpr double playerMass = 1.0;
inline constexpr double spawnHeight = 2.0;
inline constexpr double spawnSpacing = 1.5;
inline constexpr std::uint32_t spawnColumns = 8;
inline constexpr double movementForce = 10.0;

// Player IDs are assigned by the server and are nonzero.
inline constexpr double spawnX(std::uint32_t playerId) {
    return static_cast<double>((playerId - 1) % spawnColumns) * spawnSpacing;
}
} // namespace sandbox::simulation