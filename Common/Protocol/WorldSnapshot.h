#pragma once

#include "Common/Math/Quaternion.h"
#include "Common/SimulationConfig.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MoongTangE {

inline constexpr std::size_t MAX_PLAYERS = simulation::MAX_PLAYERS;
inline constexpr std::size_t MAX_COLLECTIBLES = simulation::MAX_COLLECTIBLES;

enum class CollectibleKind : std::uint32_t
{
    APPLE = 1,
    BANANA = 2,
    BURGER = 3,
    DONUT = 4
};

inline constexpr bool valid_collectible_kind(CollectibleKind kind)
{
    return kind >= CollectibleKind::APPLE && kind <= CollectibleKind::DONUT;
}

enum class RoundPhase : std::uint32_t
{
    FREE_PLAY = 0, // Standalone physics snapshots; hosted games always have a round.
    COUNTDOWN = 1,
    PLAYING = 2,
    RESULTS = 3
};

struct RoundState
{
    std::uint64_t id{};
    RoundPhase phase{RoundPhase::FREE_PLAY};
    std::uint64_t remainingTicks{};
    bool competitive{};
};

struct PlayerState
{
    std::uint32_t id{};
    Vector3 position{}, velocity{};
    Quaternion orientation{};
    Vector3 angularVelocity{};
    std::string nickname;
    // Zero while alive; otherwise the authoritative tick of this particular defeat.
    std::uint64_t deathTick{};
    std::uint32_t roundScore{};
    bool roundWinner{};
    double radius{simulation::PLAYER_RADIUS};
    std::uint32_t foodEaten{};
    std::uint64_t mineHitTick{};
};

struct CollectibleState
{
    std::uint32_t id{};
    CollectibleKind kind{CollectibleKind::APPLE};
    Vector3 position{};
    Quaternion orientation{};
    double scale{1};
    std::uint32_t attachedPlayerId{};
};

struct MineState
{
    std::uint32_t id{};
    Vector3 position{};
    std::uint64_t readyTick{};
};

struct WorldSnapshot
{
    std::uint64_t tick{};
    std::vector<PlayerState> players;
    std::vector<CollectibleState> collectibles;
    RoundState round;
    std::vector<MineState> mines;
};

} // namespace MoongTangE
