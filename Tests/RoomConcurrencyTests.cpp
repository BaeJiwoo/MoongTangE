#include "Server/Core/ServerHost.h"
#include "TestSupport.h"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <set>
#include <thread>
#include <vector>

namespace {

using namespace MoongTangE;
using namespace std::chrono_literals;

std::size_t player_count(const ServerHost& host)
{
    std::size_t count = 0;
    for (const auto& room : host.Snapshots()) {
        MOONGTANGE_CHECK(room.world.players.size() <= 3);
        count += room.world.players.size();
    }
    return count;
}

void simultaneous_admission_never_overbooks()
{
    ServerHost host({.m_maxRooms = 3, .m_emptyRoomLifetime = 0s});
    host.Start();
    constexpr std::size_t REQUESTS = 40;
    std::barrier gate(static_cast<std::ptrdiff_t>(REQUESTS + 1));
    std::vector<std::uint32_t> requests(REQUESTS);
    std::vector<std::jthread> workers;
    for (std::size_t index = 0; index < REQUESTS; ++index) {
        workers.emplace_back([&, index] {
            gate.arrive_and_wait();
            requests[index] = host.Connect();
        });
    }
    gate.arrive_and_wait();
    workers.clear();
    MOONGTANGE_CHECK(test::wait_for([&] {
        return std::ranges::all_of(requests, [&](auto id) {
            return host.Admission(id).status != AdmissionStatus::PENDING;
        });
    }));

    std::set<std::uint32_t> accepted;
    for (const auto request : requests) {
        const auto admission = host.Admission(request);
        if (admission.status == AdmissionStatus::ACCEPTED) {
            MOONGTANGE_CHECK(accepted.insert(admission.playerId).second);
            MOONGTANGE_CHECK(admission.roomId >= 1 && admission.roomId <= 3);
        } else {
            MOONGTANGE_CHECK(admission.playerId == 0 && admission.roomId == 0);
        }
    }
    MOONGTANGE_CHECK(accepted.size() == 9);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return player_count(host) == 9;
    }));
    for (const auto request : requests) {
        host.Disconnect(request);
        host.Disconnect(request);
    }
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Snapshots().empty();
    }));
}

void cancelled_requests_and_restart_leave_no_ghosts()
{
    ServerHost host({.m_maxRooms = 1, .m_emptyRoomLifetime = 0s});
    host.Start();
    std::vector<std::uint32_t> cancelled;
    for (int index = 0; index < 200; ++index) {
        const auto request = host.Connect();
        MOONGTANGE_CHECK(request != 0);
        cancelled.push_back(request);
        host.Disconnect(request);
        MOONGTANGE_CHECK(!host.Submit({request, 1, 1, 0}));
        MOONGTANGE_CHECK(!host.SnapshotFor(request));
    }
    const auto survivor = host.Connect();
    MOONGTANGE_CHECK(survivor != 0);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.HasClient(survivor) && player_count(host) == 1;
    }));
    const auto room = host.SnapshotFor(survivor);
    MOONGTANGE_CHECK(room && room->world.players.front().id == survivor);
    const auto oldRoomId = room->roomId;
    host.Disconnect(survivor);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Snapshots().empty();
    }));
    const auto replacement = host.Connect();
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.HasClient(replacement);
    }));
    MOONGTANGE_CHECK(host.Admission(replacement).roomId > oldRoomId);

    host.Stop();
    host.Start();
    const auto restarted = host.Connect();
    MOONGTANGE_CHECK(restarted > replacement);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.HasClient(restarted);
    }));
    for (const auto request : cancelled) {
        MOONGTANGE_CHECK(host.Admission(request).status == AdmissionStatus::UNAVAILABLE);
    }
    MOONGTANGE_CHECK(!host.HasClient(replacement) && !host.Submit({replacement, 2, 1, 0}));
}

void input_flood_cannot_fill_other_players_slots()
{
    ServerHost host({.m_maxRooms = 2});
    host.Start();
    std::vector<std::uint32_t> players;
    for (int index = 0; index < 4; ++index) {
        players.push_back(host.Connect());
    }
    MOONGTANGE_CHECK(test::wait_for([&] {
        return player_count(host) == players.size();
    }));
    MOONGTANGE_CHECK(host.Admission(players[0]).roomId != host.Admission(players[3]).roomId);

    std::atomic<bool> accepted{true};
    std::jthread flood([&](std::stop_token stop) {
        for (std::uint64_t sequence = 1; !stop.stop_requested(); ++sequence) {
            if (!host.Submit({players[0], sequence, 1, 0})) {
                accepted = false;
                break;
            }
            if (sequence % 256 == 0) {
                std::this_thread::sleep_for(1ms);
            }
        }
    });
    MOONGTANGE_CHECK(host.Submit({players[3], 1, -1, 0}));
    MOONGTANGE_CHECK(test::wait_for([&] {
        MOONGTANGE_CHECK(host.Pending() <= players.size());
        const auto room = host.SnapshotFor(players[3]);
        return room && room->world.players.front().velocity.x < -0.5;
    }));
    flood.request_stop();
    flood.join();
    MOONGTANGE_CHECK(accepted);
    MOONGTANGE_CHECK(!host.Submit({players[3], 1, 1, 0}));
    host.Disconnect(players[0]);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return player_count(host) == 3;
    }));
}

void configured_capacity_is_not_the_old_global_world_limit()
{
    ServerHost host({.m_maxRooms = GameRoomOptions::MAX_ROOM_LIMIT});
    host.Start();
    std::vector<std::uint32_t> players;
    for (std::size_t index = 0; index < host.Capacity(); ++index) {
        const auto request = host.Connect();
        MOONGTANGE_CHECK(request != 0);
        players.push_back(request);
    }
    MOONGTANGE_CHECK(test::wait_for([&] {
        return player_count(host) == host.Capacity();
    }));
    MOONGTANGE_CHECK(host.Snapshots().size() == GameRoomOptions::MAX_ROOM_LIMIT);
    MOONGTANGE_CHECK(host.Capacity() == 768);
    const auto excess = host.Connect();
    MOONGTANGE_CHECK(excess != 0);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Admission(excess).status == AdmissionStatus::FULL;
    }));
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        simultaneous_admission_never_overbooks();
        cancelled_requests_and_restart_leave_no_ghosts();
        input_flood_cannot_fill_other_players_slots();
        configured_capacity_is_not_the_old_global_world_limit();
    });
}
