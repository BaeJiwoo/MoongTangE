#pragma once

#include "Common/Protocol/RoomSnapshot.h"
#include "Server/Core/GameRoom.h"
#include "Server/Core/GameRoomOptions.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace MoongTangE {

// Creation-order first fit. No sockets, threads or externally mutable room access.
class GameRoomManager
{
  public:
    using Clock = GameRoom::Clock;

    explicit GameRoomManager(GameRoomOptions options = {});

    std::uint64_t Join(std::uint32_t playerId, Clock::time_point now);
    void Leave(std::uint32_t playerId, Clock::time_point now);
    bool Apply(InputCommand input);
    void Step(Clock::time_point now);

    std::uint64_t RoomFor(std::uint32_t playerId) const;
    std::size_t RoomCount() const;
    std::size_t PlayerCount() const;
    std::optional<RoomSnapshot> Snapshot(std::uint64_t roomId) const;
    std::vector<RoomSnapshot> Snapshots() const;

  private:
    void ExpireRooms(Clock::time_point now);

    GameRoomOptions m_options;
    std::map<std::uint64_t, GameRoom> m_rooms;
    std::map<std::uint32_t, std::uint64_t> m_memberships;
    std::uint64_t m_nextRoomId{1};
};

} // namespace MoongTangE
