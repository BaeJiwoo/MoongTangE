#include "Server/Core/ServerHost.h"
#include "Server/Core/TickScheduler.h"
#include "TestSupport.h"

#include <algorithm>
#include <cstdint>

int main()
{
    return MoongTangE::test::run([] {
        using namespace std::chrono_literals;

        MoongTangE::TickScheduler scheduler(MoongTangE::TickScheduler::Clock::time_point{});
        MOONGTANGE_CHECK(scheduler.Advance({}) == MoongTangE::TickScheduler::Clock::time_point{} +
                                                      MoongTangE::TickScheduler::TICK_PERIOD);

        MoongTangE::ServerHost server;
        MOONGTANGE_CHECK(server.Connect() == 0);
        server.Start();
        server.Start();
        const auto id = server.Connect();
        MOONGTANGE_CHECK(id != 0);
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            return server.HasClient(id);
        }));
        MOONGTANGE_CHECK(server.Submit({id, 1, 1, 0}));
        MOONGTANGE_CHECK(!server.Submit({id, 1, 1, 0}));
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            const auto s = server.SnapshotFor(id);
            return s && !s->world.players.empty() && s->world.players[0].velocity.x > 0.5;
        }));
        auto state = server.SnapshotFor(id)->world;
        MOONGTANGE_CHECK(state.tick != 0);
        MOONGTANGE_CHECK(state.players[0].angularVelocity.z < 0 &&
                         state.players[0].orientation.Unit());

        for (std::uint64_t seq = 2; seq < 10000; ++seq) {
            server.Submit({id, seq, 0, 0});
        }
        MOONGTANGE_CHECK(server.Pending() <= MoongTangE::ServerHost::MAX_PENDING_COMMANDS);

        server.Disconnect(id);
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            return std::ranges::all_of(server.Snapshots(), [](const auto& room) {
                return room.world.players.empty();
            });
        }));

        server.Stop();
        server.Stop();
        MOONGTANGE_CHECK(!server.Running());
        MOONGTANGE_CHECK(!server.Submit({id, 10001, 0, 0}));

        server.Start();
        const auto newId = server.Connect();
        MOONGTANGE_CHECK(newId != 0 && newId != id);
        MOONGTANGE_CHECK(MoongTangE::test::wait_for([&] {
            const auto room = server.SnapshotFor(newId);
            return room && !room->world.players.empty();
        }));

        server.Stop();
    });
}
