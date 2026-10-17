#pragma once
#include "Common/Math/Quaternion.h"
#include <cstdint>
#include <vector>
namespace sandbox {
inline constexpr std::size_t maxPlayers = 32;
struct PlayerState {
    std::uint32_t id{};
    Vector3 position{}, velocity{};
    Quaternion orientation{};
    Vector3 angularVelocity{};
};
struct WorldSnapshot {
    std::uint64_t tick{};
    std::vector<PlayerState> players;
};
}
