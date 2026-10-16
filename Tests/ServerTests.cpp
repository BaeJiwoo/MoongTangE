#include "Server/Core/ServerHost.h"
#include "Server/Core/TickScheduler.h"
#include "TestSupport.h"
int main() { return test::run([] {
    using namespace std::chrono_literals;
    sandbox::TickScheduler scheduler(sandbox::TickScheduler::Clock::time_point{});
    CHECK(scheduler.advance({}) == sandbox::TickScheduler::Clock::time_point{}+sandbox::TickScheduler::period);
    sandbox::ServerHost server;
    CHECK(server.connect() == 0);
    server.start(); server.start();
    const auto id = server.connect(); CHECK(id != 0);
    CHECK(server.submit({id,1,1,0})); CHECK(!server.submit({id,1,1,0}));
    CHECK(test::waitFor([&] { return !server.snapshot().players.empty(); }));
    auto state = server.snapshot(); CHECK(state.tick%3 == 0);
    CHECK(state.players[0].velocity.x == 5);
    for (std::uint64_t seq=2; seq<10000; ++seq) server.submit({id,seq,0,0});
    CHECK(server.pending() <= sandbox::ServerHost::maxPending);
    server.disconnect(id);
    CHECK(test::waitFor([&] { return server.snapshot().players.empty(); }));
    server.stop(); server.stop(); CHECK(!server.running());
    CHECK(!server.submit({id,10001,0,0}));
    server.start(); const auto newId = server.connect(); CHECK(newId != 0 && newId != id);
    CHECK(test::waitFor([&] { return !server.snapshot().players.empty(); }));
    server.stop();
}); }
