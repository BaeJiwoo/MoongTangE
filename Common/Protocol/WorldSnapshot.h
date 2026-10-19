#pragma once

#include "Common/Math/Quaternion.h"
#include "Common/SimulationConfig.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MoongTangE {

inline constexpr std::size_t MAX_PLAYERS = simulation::MAX_PLAYERS;

struct PlayerState
{
    std::uint32_t id{};
    Vector3 position{}, velocity{};
    Quaternion orientation{};
    Vector3 angularVelocity{};
    std::string nickname;
};

struct WorldSnapshot
{
    std::uint64_t tick{};
    std::vector<PlayerState> players;
};

} // namespace MoongTangE
