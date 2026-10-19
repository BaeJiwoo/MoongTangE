#include "Client/ClientApplication.h"

#include "Client/Input/Keyboard.h"
#include "Client/Rendering/GraphicsWindow.h"
#include "Client/Transport/LocalTransport.h"
#include "Common/Protocol/Nickname.h"
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

std::unique_ptr<IClientTransport> make_transport(const ClientOptions& options,
                                                 MainMenuAction action, const std::string& nickname)
{
    if (action == MainMenuAction::SINGLE_PLAYER) {
        auto server = std::make_shared<ServerHost>();
        server->Start();

        // LocalTransport retains shared ownership; releasing it stops the embedded server.
        return std::make_unique<LocalTransport>(std::move(server), nickname);
    }

#ifdef MOONGTANGE_HAS_NETWORK
    if (!options.remote) {
        throw std::runtime_error("Multiplayer requires a server assignment");
    }
    return std::make_unique<TcpClientTransport>(options.remote->host, options.remote->port,
                                                options.ticket, nickname);
#else
    throw std::runtime_error("This build has TCP networking disabled");
#endif
}

} // namespace

ClientApplication::ClientApplication(ClientOptions options) : m_options(std::move(options))
{
    if (m_options.smoke) {
        m_nickname = "SmokePlayer";
    }
}

void ClientApplication::Run()
{
    GraphicsWindow window(m_options.smoke);
    if (m_options.smoke) {
        StartGame(m_options.remote ? MainMenuAction::MULTIPLAYER : MainMenuAction::SINGLE_PLAYER);
    }

    for (int frame = 0; !WindowShouldClose() && (!m_options.smoke || frame < SMOKE_FRAMES);
         ++frame) {
        if (!m_transport) {
            const auto action =
                m_renderer.DrawMainMenu(m_options.remote.has_value(), m_nickname, m_errorMessage);
            if (action == MainMenuAction::DISMISS_ERROR) {
                m_errorMessage.clear();
            } else if (action != MainMenuAction::NONE) {
                StartGame(action);
            }
            continue;
        }

        m_presentation.Advance(GetFrameTime());

        auto failure = m_transport->Error();
        if (!failure.empty()) {
            ReturnToMenu(std::move(failure));
            continue;
        }

        if (!SendInput()) {
            failure = m_transport->Error();
            ReturnToMenu(failure.empty() ? "Input was rejected by the server" : std::move(failure));
            continue;
        }
        ReceiveSnapshot();

        if (m_options.smoke) {
            ObserveSmokeProgress();
        }

        const auto status = m_transport->Connected()
                                ? "Room " + std::to_string(m_transport->RoomId())
                                : std::string("Joining...");
        m_renderer.Draw(m_presentation.Sample(), m_transport->PlayerId(),
                        m_options.remote ? "TCP dedicated server" : "Embedded server",
                        status.c_str());
    }

    if (m_options.smoke) {
        if (const auto capture = std::getenv("MOONGTANGE_SMOKE_CAPTURE")) {
            window.SaveScreenshot(capture);
        }

        ValidateSmokeProgress();
    }

    if (m_transport) {
        m_transport->Close();
    }
}

void ClientApplication::StartGame(MainMenuAction action)
{
    if (action == MainMenuAction::SINGLE_PLAYER && !valid_nickname(m_nickname)) {
        m_errorMessage = "Enter a valid nickname before starting";
        return;
    }
    try {
        m_transport = make_transport(m_options, action, m_nickname);
    } catch (const std::exception& error) {
        m_errorMessage = error.what();
    }
}

void ClientApplication::ReturnToMenu(std::string errorMessage)
{
    m_transport->Close();
    m_transport.reset();
    m_presentation.Reset();
    m_authoritativeState = {};
    m_roomId = 0;
    m_sequence = 0;
    m_errorMessage = std::move(errorMessage);
}

bool ClientApplication::SendInput()
{
    if (!m_transport->Connected()) {
        return true;
    }

    auto input = keyboard_input(m_transport->PlayerId(), ++m_sequence);
    if (m_options.smoke) {
        input.x = m_sequence < SMOKE_PUSH_FRAMES ? 1 : 0;
        input.z = 0;
    }

    return m_transport->Send(input);
}

void ClientApplication::ReceiveSnapshot()
{
    const auto roomId = m_transport->RoomId();
    if (roomId != m_roomId) {
        m_roomId = roomId;
        m_authoritativeState = {};
        m_presentation.Reset();
        m_initialX.reset();
    }

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

        if (!m_initialX) {
            m_initialX = player.position.x;
        }
        m_moved = m_moved || player.position.x > *m_initialX + simulation::PLAYER_RADIUS;
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
