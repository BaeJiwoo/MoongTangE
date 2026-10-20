#pragma once

#include "Common/SimulationConfig.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace MoongTangE {

struct GameRoomOptions
{
    static constexpr std::size_t PLAYERS_PER_ROOM = 3;
    static constexpr std::size_t MAX_ROOM_LIMIT = 256;
    static constexpr unsigned MAX_EMPTY_ROOM_SECONDS = 24 * 60 * 60;

    std::size_t m_maxRooms{8};
    std::chrono::seconds m_emptyRoomLifetime{30};
    std::uint64_t m_worldSeed{simulation::DEFAULT_WORLD_SEED};

    void Validate() const;
};

} // namespace MoongTangE
