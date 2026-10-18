#pragma once

#include "Common/Game/GameWorld.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace MoongTangE {

// All access belongs to the server tick thread.
class GameRoom
{
  public:
    using Clock = std::chrono::steady_clock;

    bool Join(std::uint32_t playerId);
    void Leave(std::uint32_t playerId, Clock::time_point now);
    bool Apply(InputCommand input);
    void Step();

    std::size_t PlayerCount() const;
    bool Expired(Clock::time_point now, std::chrono::seconds lifetime) const;
    WorldSnapshot Snapshot() const;

  private:
    GameWorld m_world;
    std::optional<Clock::time_point> m_emptySince;
};

} // namespace MoongTangE
