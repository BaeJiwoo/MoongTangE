#include "Client/ClientApplication.h"

#include "Client/Input/Keyboard.h"
#include "Client/Rendering/GraphicsWindow.h"
#include "Client/Transport/LocalTransport.h"
#include "Common/SimulationConfig.h"
#include "Server/Core/ServerHost.h"
#ifdef MOONGTANGE_HAS_NETWORK
#include "Client/Transport/TcpClientTransport.h"
#endif
#include <cstdlib>
#include <memory>
#include <raylib.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace MoongTangE {
namespace {
constexpr int SMOKE_FRAMES = 120;
constexpr std::uint64_t SMOKE_PUSH_FRAMES = 60;

std::unique_ptr<IClientTransport> make_transport(const ClientOptions& options)
{
    if (!options.remote) {
        auto server = std::make_shared<ServerHost>();
        server->Start();
        // LocalTransport retains shared ownership; releasing it stops the embedded server.
        return std::make_unique<LocalTransport>(std::move(server));
    }
#ifdef MOONGTANGE_HAS_NETWORK
    return std::make_unique<TcpClientTransport>(options.remote->host, options.remote->port);
#else
    throw std::runtime_error("This build has TCP networking disabled");
#endif
}
} // namespace

ClientApplication::ClientApplication(ClientOptions options)
    : m_options(std::move(options)), m_transport(make_transport(m_options))
{
}

void ClientApplication::Run()
{
    GraphicsWindow window(m_options.smoke);
    for (int frame = 0; !WindowShouldClose() && (!m_options.smoke || frame < SMOKE_FRAMES);
         ++frame) {
        m_presentation.Advance(GetFrameTime());
        const auto failure = m_transport->Error();
        if (!failure.empty()) {
            throw std::runtime_error(failure);
        }
        SendInput();
        ReceiveSnapshot();
        if (m_options.smoke) {
            ObserveSmokeProgress();
        }
        m_renderer.Draw(m_presentation.Sample(), m_transport->PlayerId(),
                        m_options.remote ? "TCP dedicated server" : "Embedded server",
                        m_transport->Connected() ? "Connected" : "Connecting...");
    }

    if (m_options.smoke) {
        if (const auto capture = std::getenv("MOONGTANGE_SMOKE_CAPTURE")) {
            window.SaveScreenshot(capture);
        }
        ValidateSmokeProgress();
    }
    m_transport->Close();
}

void ClientApplication::SendInput()
{
    if (!m_transport->Connected()) {
        return;
    }
    auto input = keyboard_input(m_transport->PlayerId(), ++m_sequence);
    if (m_options.smoke) {
        input.x = m_sequence < SMOKE_PUSH_FRAMES ? 1 : 0;
        input.z = 0;
    }
    if (!m_transport->Send(input)) {
        throw std::runtime_error("Input rejected: " + m_transport->Error());
    }
}

void ClientApplication::ReceiveSnapshot()
{
    if (auto snapshot = m_transport->Receive()) {
        m_authoritativeState = std::move(*snapshot);
        m_presentation.Push(m_authoritativeState);
    }
}

void ClientApplication::ObserveSmokeProgress()
{
    for (const auto& player : m_authoritativeState.players) {
        if (player.id != m_transport->PlayerId()) {
            continue;
        }
        m_moved = m_moved ||
                  player.position.x > simulation::spawn_x(player.id) + simulation::PLAYER_RADIUS;
        m_rolled = m_rolled ||
                   (player.orientation != Quaternion{} && player.angularVelocity.Length() > 0.1);
    }
}

void ClientApplication::ValidateSmokeProgress() const
{
    if (!m_moved || !m_rolled) {
        throw std::runtime_error("Smoke test did not observe server rolling");
    }
}
} // namespace MoongTangE
