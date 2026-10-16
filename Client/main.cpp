#include "Client/Input/Keyboard.h"
#include "Client/Rendering/Scene.h"
#include "Client/Transport/LocalTransport.h"
#include <iostream>
#include <string_view>
int main(int argc, char** argv) {
    bool smoke=false;
    for (int i=1; i<argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--single") continue;
        if (arg == "--smoke") { smoke=true; continue; }
        if (arg == "--help") { std::cout << "Client --single [--smoke]\n"; return 0; }
        std::cerr << "Unknown option: " << arg << '\n'; return 2;
    }
    try {
        auto server = std::make_shared<sandbox::ServerHost>();
        server->start();
        std::unique_ptr<sandbox::IClientTransport> transport = std::make_unique<sandbox::LocalTransport>(server);
        SetConfigFlags(FLAG_MSAA_4X_HINT | (smoke ? FLAG_WINDOW_HIDDEN : FLAG_WINDOW_RESIZABLE));
        InitWindow(1100,720,"Portable Physics Sandbox");
        if (!IsWindowReady()) throw std::runtime_error("Graphics window initialization failed");
        SetTargetFPS(60);
        sandbox::WorldSnapshot state;
        std::uint64_t sequence=0;
        int frame=0;
        bool moved=false;
        while (!WindowShouldClose() && (!smoke || frame<120)) {
            auto input = sandbox::keyboardInput(transport->playerId(),++sequence);
            if (smoke) { input.x = frame<60 ? 1 : 0; input.z=0; }
            if (!transport->send(input)) throw std::runtime_error("Input rejected: "+transport->error());
            if (auto snapshot = transport->receive()) state=std::move(*snapshot);
            for (const auto& p : state.players) if (p.id==transport->playerId() && p.position.x>0.5) moved=true;
            sandbox::renderScene(state,transport->playerId(),"Embedded server", "Connected");
            ++frame;
        }
        CloseWindow();
        transport->close(); server->stop();
        if (smoke && !moved) throw std::runtime_error("Smoke test did not observe server movement");
        std::cout << "Client stopped successfully\n";
        return 0;
    } catch (const std::exception& e) {
        if (IsWindowReady()) CloseWindow();
        std::cerr << e.what() << '\n'; return 1;
    }
}
