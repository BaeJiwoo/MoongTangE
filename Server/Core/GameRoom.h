#pragma once

#include "Common/Game/GameWorld.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>

namespace MoongTangE {

// All access belongs to the server tick thread.
class GameRoom
{
  public:
    using Clock = std::chrono::steady_clock;

    explicit GameRoom(std::uint64_t seed = simulation::DEFAULT_WORLD_SEED);

    bool Join(std::uint32_t playerId);
    bool Reserve(std::uint32_t playerId);
    bool Claim(std::uint32_t playerId);
    void Leave(std::uint32_t playerId, Clock::time_point now);
    bool Apply(InputCommand input);
    void Step();

    std::size_t PlayerCount() const;
    std::size_t ReservedCount() const;
    bool Expired(Clock::time_point now, std::chrono::seconds lifetime) const;
    WorldSnapshot Snapshot() const;

  private:
    GameWorld m_world;
    std::set<std::uint32_t> m_reserved;
    std::optional<Clock::time_point> m_emptySince;
};

} // namespace MoongTangE
