#include "Server/Core/ServerHost.h"
#include "Server/Core/TickScheduler.h"
#include "TestSupport.h"
#include <cstdint>

int main() {
    return MoongTangE::test::run([] {
        using namespace std::chrono_literals;
        MoongTangE::TickScheduler scheduler(MoongTangE::TickScheduler::Clock::time_point{});
        MOONGTANGE_CHECK(scheduler.advance({}) == MoongTangE::TickScheduler::Clock::time_point{} +
                                                      MoongTangE::TickScheduler::TICK_PERIOD);
        MoongTangE::ServerHost server;
        MOONGTANGE_CHECK(server.connect() == 0);
        server.start();
        server.start();
        const auto id = server.connect();
        MOONGTANGE_CHECK(id != 0);
        MOONGTANGE_CHECK(server.submit({id, 1, 1, 0}));
        MOONGTANGE_CHECK(!server.submit({id, 1, 1, 0}));
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            const auto s = server.snapshot();
            return !s.players.empty() && s.players[0].velocity.x > 0.5;
        }));
        auto state = server.snapshot();
        MOONGTANGE_CHECK(state.tick % 3 == 0);
        MOONGTANGE_CHECK(state.players[0].angularVelocity.z < 0 &&
                         state.players[0].orientation.unit());
        for (std::uint64_t seq = 2; seq < 10000; ++seq) {
            server.submit({id, seq, 0, 0});
        }
        MOONGTANGE_CHECK(server.pending() <= MoongTangE::ServerHost::MAX_PENDING_COMMANDS);
        server.disconnect(id);
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            return server.snapshot().players.empty();
        }));
        server.stop();
        server.stop();
        MOONGTANGE_CHECK(!server.running());
        MOONGTANGE_CHECK(!server.submit({id, 10001, 0, 0}));
        server.start();
        const auto newId = server.connect();
        MOONGTANGE_CHECK(newId != 0 && newId != id);
        MOONGTANGE_CHECK(MoongTangE::test::waitFor([&] {
            return !server.snapshot().players.empty();
        }));
        server.stop();
    });
}
