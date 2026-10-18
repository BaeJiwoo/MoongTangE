#include "Server/Core/GameRoomOptions.h"

#include <stdexcept>

namespace MoongTangE {

void GameRoomOptions::Validate() const
{
    if (m_maxRooms == 0 || m_maxRooms > MAX_ROOM_LIMIT ||
        m_emptyRoomLifetime < std::chrono::seconds::zero() ||
        m_emptyRoomLifetime > std::chrono::hours(24)) {
        throw std::invalid_argument("Invalid game room limits");
    }
}

} // namespace MoongTangE
