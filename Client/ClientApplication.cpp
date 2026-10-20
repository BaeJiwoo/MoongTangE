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

#include <algorithm>
#include <cmath>
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
constexpr double DISPLAY_CONFIRMATION_SECONDS = 15;

std::unique_ptr<IClientTransport> make_transport(const ClientOptions& options, MenuAction action,
                                                 const std::string& nickname)
{
    if (action == MenuAction::SINGLE_PLAYER) {
        GameRoomOptions rooms;
        rooms.m_worldSeed = options.worldSeed;
        auto server = std::make_shared<ServerHost>(rooms);
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
    } else if (const auto savedSettings = m_displaySettingsStore.Load()) {
        m_displaySettings.LoadConfirmed(*savedSettings);
    }
}

void ClientApplication::Run()
{
    GraphicsWindow window(m_options.smoke);
    SceneRenderer renderer;
    if (m_options.smoke) {
        StartGame(m_options.remote ? MenuAction::MULTIPLAYER : MenuAction::SINGLE_PLAYER);
    } else {
        m_displaySettings.ClampMonitorCount(GetMonitorCount());
        window.ApplyDisplaySettings(m_displaySettings.Active());
    }

    for (int frame = 0; !WindowShouldClose() && (!m_options.smoke || frame < SMOKE_FRAMES);
         ++frame) {
        if (m_displayConfirmationDeadline && GetTime() >= *m_displayConfirmationDeadline) {
            RevertDisplaySettings(window);
        }
        const int confirmationSeconds =
            m_displayConfirmationDeadline
                ? std::max(0,
                           static_cast<int>(std::ceil(*m_displayConfirmationDeadline - GetTime())))
                : 0;

        const bool escapePressed = IsKeyPressed(KEY_ESCAPE);
        if (m_transport && escapePressed && !m_optionsOpen) {
            m_gameMenuOpen = !m_gameMenuOpen;
        } else if (!m_transport && escapePressed && !m_optionsOpen && m_errorMessage.empty()) {
            break;
        }

        if (!m_transport) {
            const auto action = renderer.DrawMainMenu(
                m_options.remote.has_value(), m_optionsOpen, m_displaySettings.Pending(),
                m_displayConfirmationDeadline.has_value(), confirmationSeconds, m_nickname,
                m_errorMessage);
            if (HandleMenuAction(action, window)) {
                break;
            }
            continue;
        }

        m_presentation.Advance(GetFrameTime());

        auto failure = m_transport->Error();
        if (!failure.empty()) {
            ReturnToMenu(std::move(failure));
            continue;
        }

        if (!SendInput(!m_gameMenuOpen)) {
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
        const auto gameMenuAction = renderer.Draw(
            m_presentation.Sample(), m_transport->PlayerId(),
            m_options.remote ? "TCP dedicated server" : "Embedded server", status.c_str(),
            m_gameMenuOpen, m_optionsOpen, m_displaySettings.Pending(),
            m_displayConfirmationDeadline.has_value(), confirmationSeconds);
        if (HandleMenuAction(gameMenuAction, window)) {
            break;
        }
    }

    if (m_options.smoke) {
        if (const auto capture = std::getenv("MOONGTANGE_SMOKE_CAPTURE")) {
            window.SaveScreenshot(capture);
        }

        ValidateSmokeProgress();
    }

    CloseTransport();
}

bool ClientApplication::HandleMenuAction(MenuAction action, GraphicsWindow& window)
{
    switch (action) {
    case MenuAction::NONE:
        break;
    case MenuAction::SINGLE_PLAYER:
    case MenuAction::MULTIPLAYER:
        StartGame(action);
        break;
    case MenuAction::RESUME_GAME:
        m_gameMenuOpen = false;
        break;
    case MenuAction::EXIT_GAME:
        CloseTransport();
        return true;
    case MenuAction::SHOW_OPTIONS:
        m_displaySettings.Cancel();
        m_optionsOpen = true;
        break;
    case MenuAction::PREVIOUS_WINDOW_MODE:
        m_displaySettings.SelectPreviousWindowMode();
        break;
    case MenuAction::NEXT_WINDOW_MODE:
        m_displaySettings.SelectNextWindowMode();
        break;
    case MenuAction::PREVIOUS_MONITOR:
        m_displaySettings.SelectPreviousMonitor(GetMonitorCount());
        break;
    case MenuAction::NEXT_MONITOR:
        m_displaySettings.SelectNextMonitor(GetMonitorCount());
        break;
    case MenuAction::PREVIOUS_RESOLUTION:
        m_displaySettings.SelectPreviousResolution();
        break;
    case MenuAction::NEXT_RESOLUTION:
        m_displaySettings.SelectNextResolution();
        break;
    case MenuAction::TOGGLE_VSYNC:
        m_displaySettings.ToggleVSync();
        break;
    case MenuAction::PREVIOUS_FRAME_RATE_LIMIT:
        m_displaySettings.SelectPreviousFrameRateLimit();
        break;
    case MenuAction::NEXT_FRAME_RATE_LIMIT:
        m_displaySettings.SelectNextFrameRateLimit();
        break;
    case MenuAction::APPLY_OPTIONS:
        if (m_displaySettings.Pending() == m_displaySettings.Active()) {
            m_optionsOpen = false;
        } else {
            m_displaySettings.Apply();
            window.ApplyDisplaySettings(m_displaySettings.Active());
            m_displayConfirmationDeadline = GetTime() + DISPLAY_CONFIRMATION_SECONDS;
        }
        break;
    case MenuAction::CANCEL_OPTIONS:
        m_displaySettings.Cancel();
        m_optionsOpen = false;
        break;
    case MenuAction::CONFIRM_DISPLAY_SETTINGS:
        ConfirmDisplaySettings();
        break;
    case MenuAction::REVERT_DISPLAY_SETTINGS:
        RevertDisplaySettings(window);
        break;
    case MenuAction::DISMISS_ERROR:
        m_errorMessage.clear();
        break;
    }
    return false;
}

void ClientApplication::ConfirmDisplaySettings()
{
    m_displaySettings.Confirm();
    m_displayConfirmationDeadline.reset();
    m_optionsOpen = false;

    if (!m_displaySettingsStore.Save(m_displaySettings.Confirmed())) {
        m_errorMessage = "Display settings were kept but could not be saved";
    }
}

void ClientApplication::RevertDisplaySettings(GraphicsWindow& window)
{
    m_displaySettings.Revert();
    window.ApplyDisplaySettings(m_displaySettings.Active());
    m_displayConfirmationDeadline.reset();
}

void ClientApplication::StartGame(MenuAction action)
{
    if (action == MenuAction::SINGLE_PLAYER && !valid_nickname(m_nickname)) {
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
    CloseTransport();
    m_presentation.Reset();
    m_authoritativeState = {};
    m_roomId = 0;
    m_sequence = 0;
    m_errorMessage = std::move(errorMessage);
}

void ClientApplication::CloseTransport()
{
    if (!m_transport) {
        return;
    }

    m_transport->Close();
    m_transport.reset();
    m_gameMenuOpen = false;
    m_optionsOpen = false;
}

bool ClientApplication::SendInput(bool movementEnabled)
{
    if (!m_transport->Connected()) {
        return true;
    }

    auto input = keyboard_input(m_transport->PlayerId(), ++m_sequence);
    if (!movementEnabled) {
        input.x = 0;
        input.z = 0;
    } else if (m_options.smoke) {
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
