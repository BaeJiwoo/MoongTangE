#include "Server/Core/GameRoomManager.h"

namespace MoongTangE {

GameRoomManager::GameRoomManager(GameRoomOptions options) : m_options(options)
{
    m_options.Validate();
}

std::uint64_t GameRoomManager::Join(std::uint32_t playerId, Clock::time_point now)
{
    return Allocate(playerId, now, false);
}

std::uint64_t GameRoomManager::Reserve(std::uint32_t playerId, Clock::time_point now)
{
    return Allocate(playerId, now, true);
}

bool GameRoomManager::Claim(std::uint32_t playerId)
{
    const auto roomId = RoomFor(playerId);
    return roomId != 0 && m_rooms.at(roomId).Claim(playerId);
}

std::uint64_t GameRoomManager::Allocate(std::uint32_t playerId, Clock::time_point now, bool reserve)
{
    if (playerId == 0 || m_memberships.contains(playerId)) {
        return 0;
    }
    ExpireRooms(now);

    for (auto& [roomId, room] : m_rooms) {
        if (reserve ? room.Reserve(playerId) : room.Join(playerId)) {
            m_memberships.emplace(playerId, roomId);
            return roomId;
        }
    }

    if (m_rooms.size() >= m_options.m_maxRooms || m_nextRoomId == 0) {
        return 0;
    }
    const auto roomId = m_nextRoomId++;
    auto& room = m_rooms.try_emplace(roomId).first->second;
    if (reserve) {
        room.Reserve(playerId);
    } else {
        room.Join(playerId);
    }
    m_memberships.emplace(playerId, roomId);
    return roomId;
}

void GameRoomManager::Leave(std::uint32_t playerId, Clock::time_point now)
{
    const auto membership = m_memberships.find(playerId);
    if (membership == m_memberships.end()) {
        return;
    }
    m_rooms.at(membership->second).Leave(playerId, now);
    m_memberships.erase(membership);
}

bool GameRoomManager::Apply(InputCommand input)
{
    const auto membership = m_memberships.find(input.playerId);
    return membership != m_memberships.end() && m_rooms.at(membership->second).Apply(input);
}

void GameRoomManager::Step(Clock::time_point now)
{
    ExpireRooms(now);
    for (auto& [roomId, room] : m_rooms) {
        (void)roomId;
        room.Step();
    }
}

std::uint64_t GameRoomManager::RoomFor(std::uint32_t playerId) const
{
    const auto membership = m_memberships.find(playerId);
    return membership == m_memberships.end() ? 0 : membership->second;
}

std::size_t GameRoomManager::RoomCount() const
{
    return m_rooms.size();
}

std::size_t GameRoomManager::PlayerCount() const
{
    std::size_t players = 0;
    for (const auto& [roomId, room] : m_rooms) {
        (void)roomId;
        players += room.PlayerCount();
    }
    return players;
}

std::vector<RoomOccupancy> GameRoomManager::Occupancy() const
{
    std::vector<RoomOccupancy> occupancy;
    for (const auto& [roomId, room] : m_rooms) {
        occupancy.push_back({roomId, room.PlayerCount(), room.ReservedCount()});
    }
    return occupancy;
}

std::optional<RoomSnapshot> GameRoomManager::Snapshot(std::uint64_t roomId) const
{
    const auto room = m_rooms.find(roomId);
    if (room == m_rooms.end()) {
        return {};
    }
    return RoomSnapshot{roomId, room->second.Snapshot()};
}

std::vector<RoomSnapshot> GameRoomManager::Snapshots() const
{
    std::vector<RoomSnapshot> snapshots;
    snapshots.reserve(m_rooms.size());
    for (const auto& [roomId, room] : m_rooms) {
        snapshots.push_back({roomId, room.Snapshot()});
    }
    return snapshots;
}

void GameRoomManager::ExpireRooms(Clock::time_point now)
{
    std::erase_if(m_rooms, [&](const auto& entry) {
        return entry.second.Expired(now, m_options.m_emptyRoomLifetime);
    });
}

} // namespace MoongTangE
