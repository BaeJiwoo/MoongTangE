#include "Client/Transport/TcpClientTransport.h"
#include "Server/Network/AsioServer.h"
#include "Network/TcpChannel.h"
#include "TestSupport.h"
namespace {
sandbox::wire::Bytes readFrame(asio::ip::tcp::socket& socket) {
    std::array<std::uint8_t,4> header;
    asio::read(socket,asio::buffer(header));
    sandbox::wire::Bytes data(sandbox::wire::frameSize(header));
    asio::read(socket,asio::buffer(data)); return data;
}
bool closed(asio::ip::tcp::socket& socket) {
    socket.non_blocking(true);
    return test::waitFor([&] {
        std::array<char,4096> bytes{}; asio::error_code error;
        socket.read_some(asio::buffer(bytes),error);
        return error && error!=asio::error::would_block && error!=asio::error::try_again;
    });
}
}
int main() { return test::run([] {
    using namespace sandbox;
    auto core=std::make_shared<ServerHost>(); core->start();
    AsioServer server(core,"127.0.0.1",0);
    TcpClientTransport a("127.0.0.1",server.port()), b("127.0.0.1",server.port());
    CHECK(test::waitFor([&] { return a.connected() && b.connected(); }));
    const auto aId=a.playerId();
    CHECK(aId && aId!=b.playerId());
    CHECK(!a.send({b.playerId(),1,1,0}));
    CHECK(a.send({aId,1,1,0})); CHECK(b.send({b.playerId(),1,0,-1}));
    CHECK(test::waitFor([&] {
        if (auto state=a.receive()) for (const auto& p:state->players) if (p.id==aId && p.velocity.x>0.5) {
            CHECK(test::approx(p.angularVelocity.z,-p.velocity.x/0.5));
            CHECK(p.orientation!=Quaternion{} && p.orientation.unit()); return true;
        }
        return false;
    }));
    asio::io_context io;
    const asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"),server.port());
    {
        asio::ip::tcp::socket raw(io); raw.connect(endpoint);
        const auto greeting=wire::frame(wire::hello());
        for (const auto byte:greeting) asio::write(raw,asio::buffer(&byte,1));
        const auto id=wire::readHello(readFrame(raw),true); CHECK(id);
        const auto command=wire::frame(wire::input({id,1,1,0}));
        for (const auto byte:command) asio::write(raw,asio::buffer(&byte,1));
        bool moved=false;
        for (int i=0; i<40 && !moved; ++i) {
            const auto state=wire::readSnapshot(readFrame(raw));
            for (const auto& p:state.players) if (p.id==id && p.velocity.x>0.5 && p.angularVelocity.z<0) moved=true;
        }
        CHECK(moved);
        const auto goodbye=wire::frame(wire::Bytes{5}); asio::write(raw,asio::buffer(goodbye));
        CHECK(closed(raw));
    }
    for (const auto& bad : std::vector<wire::Bytes>{{0,0,16,1},{0,0,0,0},wire::frame(wire::Bytes{1,0,1}),wire::frame(wire::input({1,1,0,0}))}) {
        asio::ip::tcp::socket raw(io); raw.connect(endpoint);
        asio::write(raw,asio::buffer(bad)); CHECK(closed(raw));
    }
    a.close(); a.close(); b.close();
    CHECK(test::waitFor([&] { return core->snapshot().players.empty(); }));
    { TcpClientTransport abandoned("127.0.0.1",server.port()); abandoned.close(); }
    asio::ip::tcp::acceptor reserve(io,{asio::ip::make_address("127.0.0.1"),0});
    const auto unused=reserve.local_endpoint().port(); reserve.close();
    TcpClientTransport refused("127.0.0.1",unused);
    CHECK(test::waitFor([&] { return !refused.error().empty(); }));
    CHECK(!refused.connected());
    for (int i=0; i<5; ++i) {
        auto shutting=std::make_unique<AsioServer>(core,"127.0.0.1",0);
        TcpClientTransport first("127.0.0.1",shutting->port()), second("127.0.0.1",shutting->port());
        CHECK(test::waitFor([&] { return first.connected() && second.connected(); }));
        shutting.reset();
        CHECK(test::waitFor([&] { return !first.connected() && !second.connected(); }));
    }
}); }
