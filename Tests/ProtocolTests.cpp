#include "Common/Protocol/Wire.h"
#include "TestSupport.h"
#include <limits>
int main() { return test::run([] {
    using namespace sandbox;
    const auto hello=wire::hello();
    CHECK(hello==wire::Bytes{1,0,2});
    CHECK(wire::frame(hello)==wire::Bytes{0,0,0,3,1,0,2});
    CHECK(test::throws([] { wire::readHello(wire::Bytes{1,0,1},false); }));
    CHECK(wire::readHello(wire::hello(42),true)==42);
    auto bad=hello; bad[2]=99;
    CHECK(test::throws([&] { wire::readHello(bad,false); }));
    InputCommand input{7,0x0102030405060708,0.5,-1};
    auto bytes=wire::input(input);
    CHECK(bytes[1]==1 && bytes[8]==8);
    const auto restored=wire::readInput(bytes,7);
    CHECK(restored.sequence==input.sequence && restored.x==input.x && restored.z==input.z);
    for (std::size_t i=0; i<bytes.size(); ++i)
        CHECK(test::throws([&] { wire::readInput(std::span(bytes).first(i),7); }));
    bad=bytes; bad.push_back(0); CHECK(test::throws([&] { wire::readInput(bad,7); }));
    bad=bytes; bad[9]=0x7f; bad[10]=0xf8; for (int i=11; i<17; ++i) bad[i]=0;
    CHECK(test::throws([&] { wire::readInput(bad,7); }));
    CHECK(test::throws([] { wire::frameSize(wire::Bytes{0,0,0,0}); }));
    CHECK(test::throws([] { wire::frameSize(wire::Bytes{0,0,16,1}); }));
    CHECK(test::throws([] { wire::frame(wire::Bytes(wire::maxFrame+1)); }));
    WorldSnapshot state{42,{{1,{2,0.5,3},{4,0,5}},{2,{}, {}}}};
    state.players[0].orientation=Quaternion::fromRotation({0.3,0.2,-0.7});
    state.players[0].angularVelocity={10,0,-8};
    const auto decoded=wire::readSnapshot(wire::snapshot(state));
    CHECK(decoded.tick==42 && decoded.players.size()==2);
    CHECK(decoded.players[0].position==state.players[0].position);
    CHECK(decoded.players[0].velocity==state.players[0].velocity);
    CHECK(decoded.players[0].orientation==state.players[0].orientation);
    CHECK(decoded.players[0].angularVelocity==state.players[0].angularVelocity);
    auto snap=wire::snapshot(state);
    for (std::size_t i=0; i<snap.size(); ++i)
        CHECK(test::throws([&] { wire::readSnapshot(std::span(snap).first(i)); }));
    bad=snap; std::fill(bad.begin()+63,bad.begin()+95,std::uint8_t{0}); // Zero quaternion.
    CHECK(test::throws([&] { wire::readSnapshot(bad); }));
    bad[63]=0x40; // Finite but non-unit quaternion (w=2).
    CHECK(test::throws([&] { wire::readSnapshot(bad); }));
    bad=snap; bad[95]=0x7f; bad[96]=0xf8; // NaN angular velocity.
    CHECK(test::throws([&] { wire::readSnapshot(bad); }));
    state.players[0].orientation={0,0,0,0};
    CHECK(test::throws([&] { wire::snapshot(state); }));
    state.players[0].orientation={};
    state.players[0].angularVelocity.x=std::numeric_limits<double>::infinity();
    CHECK(test::throws([&] { wire::snapshot(state); }));
    state.players[0].angularVelocity={};
    state.players[1].id=1; CHECK(test::throws([&] { wire::snapshot(state); }));
    state.players.clear();
    for (std::uint32_t id=1;id<=maxPlayers;++id) state.players.push_back({id,{},{}});
    const auto full=wire::snapshot(state);
    CHECK(full.size()==3467 && full.size()<=wire::maxFrame);
    CHECK(wire::readSnapshot(full).players.size()==maxPlayers);
}); }
