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

struct PlayerState
{
    std::uint32_t id{};
    Vector3 position{}, velocity{};
    Quaternion orientation{};
    Vector3 angularVelocity{};
    std::string nickname;
};

struct CollectibleState
{
    std::uint32_t id{};
    CollectibleKind kind{CollectibleKind::APPLE};
    Vector3 position{}, velocity{};
    Quaternion orientation{};
    Vector3 angularVelocity{};
    double scale{1};
};

struct WorldSnapshot
{
    std::uint64_t tick{};
    std::vector<PlayerState> players;
    std::vector<CollectibleState> collectibles;
};

} // namespace MoongTangE
