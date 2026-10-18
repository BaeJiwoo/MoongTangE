#include "Server/Core/GameRoomManager.h"
#include "TestSupport.h"

#include <chrono>

namespace {

using namespace MoongTangE;
using namespace std::chrono_literals;

void sequential_admission_and_isolation()
{
    GameRoomManager rooms({.m_maxRooms = 3});
    const auto now = GameRoomManager::Clock::time_point{};
    MOONGTANGE_CHECK(rooms.RoomCount() == 0);
    MOONGTANGE_CHECK(rooms.Join(0, now) == 0);
    for (std::uint32_t playerId = 1; playerId <= 9; ++playerId) {
        MOONGTANGE_CHECK(rooms.Join(playerId, now) == (playerId - 1) / 3 + 1);
    }
    MOONGTANGE_CHECK(rooms.Join(1, now) == 0 && rooms.Join(10, now) == 0);
    MOONGTANGE_CHECK(rooms.RoomCount() == 3 && rooms.PlayerCount() == 9);
    MOONGTANGE_CHECK(rooms.Snapshot(2)->world.players.front().position.x == 0);
    MOONGTANGE_CHECK(!rooms.Snapshot(999));

    rooms.Leave(2, now);
    MOONGTANGE_CHECK(rooms.Join(100, now) == 1);
    const auto firstRoom = rooms.Snapshot(1)->world;
    MOONGTANGE_CHECK(firstRoom.players.back().id == 100);
    MOONGTANGE_CHECK(firstRoom.players.back().position.x == simulation::SPAWN_SPACING);
    MOONGTANGE_CHECK(rooms.RoomFor(100) == 1 && rooms.RoomFor(2) == 0);
    MOONGTANGE_CHECK(!rooms.Apply({2, 1, 1, 0}));
    MOONGTANGE_CHECK(rooms.Apply({100, 1, 1, 0}));
    for (int tick = 0; tick < 90; ++tick) {
        rooms.Step(now);
    }
    MOONGTANGE_CHECK(rooms.Snapshot(1)->world.players.back().velocity.x > 0);
    const auto secondRoom = rooms.Snapshot(2);
    for (const auto& player : secondRoom->world.players) {
        MOONGTANGE_CHECK(player.velocity.x == 0 && player.velocity.z == 0);
    }
    MOONGTANGE_CHECK(rooms.Snapshots().size() == 3);
}

void empty_rooms_expire_and_ids_are_not_reused()
{
    GameRoomManager rooms({.m_maxRooms = 1, .m_emptyRoomLifetime = 5s});
    const auto now = GameRoomManager::Clock::time_point{};
    const auto firstRoom = rooms.Join(1, now);
    rooms.Leave(1, now);
    rooms.Leave(1, now + 4s); // Repeated leave must not extend the deadline.
    rooms.Step(now + 4s);
    MOONGTANGE_CHECK(rooms.RoomCount() == 1);
    MOONGTANGE_CHECK(rooms.Join(2, now + 4s) == firstRoom);
    rooms.Step(now + 10s);
    MOONGTANGE_CHECK(rooms.RoomCount() == 1);
    rooms.Leave(2, now + 10s);
    rooms.Step(now + 15s);
    MOONGTANGE_CHECK(rooms.RoomCount() == 0 && rooms.PlayerCount() == 0);
    const auto secondRoom = rooms.Join(3, now + 15s);
    MOONGTANGE_CHECK(secondRoom > firstRoom && !rooms.Snapshot(firstRoom));
    rooms.Leave(3, now + 15s);
    MOONGTANGE_CHECK(rooms.Join(4, now + 20s) > secondRoom);

    GameRoomManager immediate({.m_emptyRoomLifetime = 0s});
    immediate.Join(1, now);
    immediate.Leave(1, now);
    immediate.Step(now);
    MOONGTANGE_CHECK(immediate.RoomCount() == 0);
}

void options_are_bounded()
{
    for (const auto options : {GameRoomOptions{.m_maxRooms = 0},
                               GameRoomOptions{.m_maxRooms = GameRoomOptions::MAX_ROOM_LIMIT + 1},
                               GameRoomOptions{.m_emptyRoomLifetime = -1s}}) {
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            GameRoomManager rooms(options);
        }));
    }
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        sequential_admission_and_isolation();
        empty_rooms_expire_and_ids_are_not_reused();
        options_are_bounded();
    });
}
