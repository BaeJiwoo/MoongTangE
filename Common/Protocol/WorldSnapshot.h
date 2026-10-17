#pragma once
#include "Common/Math/Vector3.h"
#include <cstdint>
#include <vector>
namespace sandbox {
inline constexpr std::size_t maxPlayers = 32;
struct PlayerState {
    std::uint32_t id{};
    Vector3 position{}, velocity{};
};
struct WorldSnapshot {
    std::uint64_t tick{};
    std::vector<PlayerState> players;
};
}
