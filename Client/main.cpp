#include "Client/Input/Keyboard.h"
#include "Client/Rendering/Scene.h"
#include "Client/Rendering/SnapshotInterpolator.h"
#include "Client/Transport/LocalTransport.h"
#include "Common/Utility/CommandLine.h"
#ifdef SANDBOX_HAS_NETWORK
#include "Client/Transport/TcpClientTransport.h"
#endif
#include <iostream>
#include <cstdlib>
#include <string_view>
int main(int argc, char** argv) {
    try {
    bool smoke=false, single=false;
    std::string remote;
    for (int i=1; i<argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--single") { single=true; continue; }
        if (arg == "--connect" && i+1<argc) { remote=argv[++i]; continue; }
        if (arg == "--smoke") { smoke=true; continue; }
        if (arg == "--help") { std::cout << "Client [--single | --connect host:port] [--smoke]\n"; return 0; }
        std::cerr << "Unknown option: " << arg << '\n'; return 2;
    }
        if (single && !remote.empty()) throw std::invalid_argument("Choose --single or --connect");
        std::shared_ptr<sandbox::ServerHost> server;
        std::unique_ptr<sandbox::IClientTransport> transport;
        if (remote.empty()) {
            server=std::make_shared<sandbox::ServerHost>(); server->start();
            transport=std::make_unique<sandbox::LocalTransport>(server);
        } else {
#ifdef SANDBOX_HAS_NETWORK
            const auto [host,port]=sandbox::endpointOption(remote);
            transport=std::make_unique<sandbox::TcpClientTransport>(host,port);
#else
            throw std::runtime_error("This build has TCP networking disabled");
#endif
        }
        SetConfigFlags(FLAG_MSAA_4X_HINT | (smoke ? FLAG_WINDOW_HIDDEN : FLAG_WINDOW_RESIZABLE));
        InitWindow(1100,720,"Portable Physics Sandbox");
        if (!IsWindowReady()) throw std::runtime_error("Graphics window initialization failed");
        SetTargetFPS(60);
        sandbox::WorldSnapshot state;
        sandbox::SnapshotInterpolator presentation;
        std::uint64_t sequence=0;
        int frame=0;
        bool moved=false, rolled=false;
        while (!WindowShouldClose() && (!smoke || frame<120)) {
            presentation.advance(GetFrameTime());
            if (!transport->error().empty()) throw std::runtime_error(transport->error());
            if (transport->connected()) {
                auto input = sandbox::keyboardInput(transport->playerId(),++sequence);
                if (smoke) { input.x = sequence<60 ? 1 : 0; input.z=0; }
                if (!transport->send(input)) throw std::runtime_error("Input rejected: "+transport->error());
            }
            if (auto snapshot = transport->receive()) {
                state=std::move(*snapshot);
                presentation.push(state);
            }
            for (const auto& p : state.players) if (p.id==transport->playerId()) {
                if (p.position.x>static_cast<double>((p.id-1)%8)*1.5+0.5) moved=true;
                if (p.orientation!=sandbox::Quaternion{} && p.angularVelocity.length()>0.1) rolled=true;
            }
            sandbox::renderScene(presentation.sample(),transport->playerId(),remote.empty() ? "Embedded server" : "TCP dedicated server",transport->connected() ? "Connected" : "Connecting...");
            ++frame;
        }
        if (smoke) if (const auto capture=std::getenv("SANDBOX_SMOKE_CAPTURE")) {
            Image screenshot=LoadImageFromScreen();
            const bool saved=ExportImage(screenshot,capture);
            UnloadImage(screenshot);
            if (!saved) throw std::runtime_error("Could not save smoke screenshot");
        }
        CloseWindow();
        transport->close(); if (server) server->stop();
        if (smoke && (!moved || !rolled)) throw std::runtime_error("Smoke test did not observe server rolling");
        std::cout << "Client stopped successfully\n";
        return 0;
    } catch (const std::exception& e) {
        if (IsWindowReady()) CloseWindow();
        std::cerr << e.what() << '\n'; return 1;
    }
}
