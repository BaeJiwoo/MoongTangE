#include "Client/ClientApplication.h"
#include "Client/Input/Keyboard.h"
#include "Client/Rendering/GraphicsWindow.h"
#include "Client/Transport/LocalTransport.h"
#include "Common/SimulationConfig.h"
#include "Server/Core/ServerHost.h"
#ifdef SANDBOX_HAS_NETWORK
#include "Client/Transport/TcpClientTransport.h"
#endif
#include <cstdlib>
#include <memory>
#include <raylib.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace sandbox {
namespace {
constexpr int smokeFrames = 120;
constexpr std::uint64_t smokePushFrames = 60;

std::unique_ptr<IClientTransport> makeTransport(const ClientOptions& options) {
    if (!options.remote) {
        auto server = std::make_shared<ServerHost>();
        server->start();
        // LocalTransport retains shared ownership; releasing it stops the embedded server.
        return std::make_unique<LocalTransport>(std::move(server));
    }
#ifdef SANDBOX_HAS_NETWORK
    return std::make_unique<TcpClientTransport>(options.remote->host, options.remote->port);
#else
    throw std::runtime_error("This build has TCP networking disabled");
#endif
}
} // namespace

ClientApplication::ClientApplication(ClientOptions options)
    : options_(std::move(options)), transport_(makeTransport(options_)) {}

void ClientApplication::run() {
    GraphicsWindow window(options_.smoke);
    for (int frame = 0; !WindowShouldClose() && (!options_.smoke || frame < smokeFrames); ++frame) {
        presentation_.advance(GetFrameTime());
        const auto failure = transport_->error();
        if (!failure.empty()) {
            throw std::runtime_error(failure);
        }
        sendInput();
        receiveSnapshot();
        if (options_.smoke) {
            observeSmokeProgress();
        }
        renderer_.draw(presentation_.sample(), transport_->playerId(),
                       options_.remote ? "TCP dedicated server" : "Embedded server",
                       transport_->connected() ? "Connected" : "Connecting...");
    }

    if (options_.smoke) {
        if (const auto capture = std::getenv("SANDBOX_SMOKE_CAPTURE")) {
            window.saveScreenshot(capture);
        }
        validateSmokeProgress();
    }
    transport_->close();
}

void ClientApplication::sendInput() {
    if (!transport_->connected()) {
        return;
    }
    auto input = keyboardInput(transport_->playerId(), ++sequence_);
    if (options_.smoke) {
        input.x = sequence_ < smokePushFrames ? 1 : 0;
        input.z = 0;
    }
    if (!transport_->send(input)) {
        throw std::runtime_error("Input rejected: " + transport_->error());
    }
}

void ClientApplication::receiveSnapshot() {
    if (auto snapshot = transport_->receive()) {
        authoritativeState_ = std::move(*snapshot);
        presentation_.push(authoritativeState_);
    }
}

void ClientApplication::observeSmokeProgress() {
    for (const auto& player : authoritativeState_.players) {
        if (player.id != transport_->playerId()) {
            continue;
        }
        moved_ =
            moved_ || player.position.x > simulation::spawnX(player.id) + simulation::PLAYER_RADIUS;
        rolled_ = rolled_ ||
                  (player.orientation != Quaternion{} && player.angularVelocity.length() > 0.1);
    }
}

void ClientApplication::validateSmokeProgress() const {
    if (!moved_ || !rolled_) {
        throw std::runtime_error("Smoke test did not observe server rolling");
    }
}
} // namespace sandbox
