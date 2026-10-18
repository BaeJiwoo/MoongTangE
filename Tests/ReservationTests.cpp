#include "Server/Core/GameRoomManager.h"
#include "Server/Core/ServerHost.h"
#include "TestSupport.h"

#include <atomic>
#include <barrier>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace MoongTangE;
using namespace std::chrono_literals;

std::string ticket_for(unsigned number)
{
    auto ticket = std::string(64, 'a');
    constexpr std::string_view HEX = "0123456789abcdef";
    for (unsigned digit = 0; digit < 8; ++digit) {
        ticket[digit] = HEX[(number >> (digit * 4)) & 15];
    }
    return ticket;
}

void reserved_seats_are_not_players()
{
    GameRoomManager rooms({2, 0s});
    const auto now = GameRoomManager::Clock::now();
    for (unsigned player = 1; player <= 3; ++player) {
        MOONGTANGE_CHECK(rooms.Reserve(player, now) == 1);
    }
    rooms.Step(now + 10s);
    MOONGTANGE_CHECK(rooms.PlayerCount() == 0 && rooms.RoomCount() == 1);
    MOONGTANGE_CHECK(rooms.Snapshot(1)->world.players.empty());
    MOONGTANGE_CHECK(rooms.Occupancy().front().reserved == 3);
    MOONGTANGE_CHECK(rooms.Join(4, now) == 2);
    MOONGTANGE_CHECK(rooms.Claim(1) && !rooms.Claim(1));
    MOONGTANGE_CHECK(rooms.PlayerCount() == 2);
    MOONGTANGE_CHECK(!rooms.Apply({2, 1, 1, 0}));
    rooms.Leave(2, now);
    MOONGTANGE_CHECK(rooms.Join(5, now) == 1);
    MOONGTANGE_CHECK(rooms.Occupancy().front().players == 2);
    MOONGTANGE_CHECK(rooms.Occupancy().front().reserved == 1);
}

void idempotency_claim_races_and_cleanup()
{
    ServerHost host({1, 0s}, true);
    host.Start();
    MOONGTANGE_CHECK(host.Connect() == 0);
    const auto ticket = ticket_for(1);
    std::barrier start(20);
    std::vector<std::jthread> threads;
    for (unsigned index = 0; index < 20; ++index) {
        threads.emplace_back([&] {
            start.arrive_and_wait();
            host.Reserve(ticket);
        });
    }
    threads.clear();
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Reservation(ticket).status == ReservationStatus::RESERVED;
    }));
    MOONGTANGE_CHECK(host.Reserve(ticket).roomId == 1);
    MOONGTANGE_CHECK(test::wait_for([&] {
        const auto state = host.Matchmaking();
        return state.rooms.size() == 1 && state.rooms.front().reserved == 1;
    }));

    std::atomic<unsigned> winner{};
    std::atomic<unsigned> winners{};
    for (unsigned index = 0; index < 20; ++index) {
        threads.emplace_back([&] {
            start.arrive_and_wait();
            if (const auto player = host.Connect(ticket)) {
                winner = player;
                ++winners;
            }
        });
    }
    threads.clear();
    MOONGTANGE_CHECK(winners == 1);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.HasClient(winner);
    }));
    MOONGTANGE_CHECK(host.Reserve(ticket).status == ReservationStatus::CONSUMED);
    MOONGTANGE_CHECK(host.CancelReservation(ticket));
    MOONGTANGE_CHECK(host.HasClient(winner));
    host.Disconnect(winner);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Matchmaking().rooms.empty();
    }));
    MOONGTANGE_CHECK(host.Connect(ticket) == 0);

    for (unsigned index = 2; index < 30; ++index) {
        const auto pendingTicket = ticket_for(index);
        host.Reserve(pendingTicket);
        MOONGTANGE_CHECK(host.CancelReservation(pendingTicket));
        MOONGTANGE_CHECK(test::wait_for([&] {
            return host.Reservation(pendingTicket).status == ReservationStatus::CANCELLED;
        }));
    }
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Matchmaking().rooms.empty();
    }));
    host.Stop();
    host.Start();
    MOONGTANGE_CHECK(host.Reservation(ticket).status == ReservationStatus::UNKNOWN);
    MOONGTANGE_CHECK(host.Connect(ticket) == 0);
}

void reservations_compete_with_direct_connections()
{
    ServerHost host({3, 0s});
    host.Start();
    std::barrier start(40);
    std::vector<std::jthread> threads;
    std::vector<unsigned> players(20);
    for (unsigned index = 0; index < 40; ++index) {
        threads.emplace_back([&, index] {
            start.arrive_and_wait();
            if (index < 20) {
                players[index] = host.Connect();
            } else {
                host.Reserve(ticket_for(index));
            }
        });
    }
    threads.clear();
    MOONGTANGE_CHECK(test::wait_for([&] {
        for (const auto player : players) {
            if (host.Admission(player).status == AdmissionStatus::PENDING) {
                return false;
            }
        }
        for (unsigned index = 20; index < 40; ++index) {
            if (host.Reservation(ticket_for(index)).status == ReservationStatus::PENDING) {
                return false;
            }
        }
        return true;
    }));
    MOONGTANGE_CHECK(test::wait_for([&] {
        const auto state = host.Matchmaking();
        unsigned occupied = 0;
        for (const auto& room : state.rooms) {
            MOONGTANGE_CHECK(room.players + room.reserved <= 3);
            occupied += static_cast<unsigned>(room.players + room.reserved);
        }
        return occupied == 9;
    }));
}

void expiration_releases_only_unclaimed_seats()
{
    ServerHost host({1, 0s}, true, 1s);
    host.Start();
    const auto expiredTicket = ticket_for(1);
    const auto consumedTicket = ticket_for(2);
    host.Reserve(expiredTicket);
    host.Reserve(consumedTicket);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Reservation(consumedTicket).status == ReservationStatus::RESERVED;
    }));
    const auto player = host.Connect(consumedTicket);
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.HasClient(player);
    }));
    MOONGTANGE_CHECK(test::wait_for([&] {
        return host.Reservation(expiredTicket).status == ReservationStatus::EXPIRED;
    }));
    MOONGTANGE_CHECK(host.Connect(expiredTicket) == 0);
    MOONGTANGE_CHECK(host.Reserve(expiredTicket).status == ReservationStatus::EXPIRED);
    MOONGTANGE_CHECK(host.HasClient(player));
    MOONGTANGE_CHECK(host.Reservation(consumedTicket).status == ReservationStatus::CONSUMED);
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        reserved_seats_are_not_players();
        idempotency_claim_races_and_cleanup();
        reservations_compete_with_direct_connections();
        expiration_releases_only_unclaimed_seats();
    });
}
