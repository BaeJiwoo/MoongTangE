#pragma once

#include "Client/ClientOptions.h"
#include "Client/Rendering/DisplaySettings.h"
#include "Client/Rendering/DisplaySettingsStore.h"
#include "Client/Rendering/Scene.h"
#include "Client/Rendering/SnapshotInterpolator.h"
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/Transport/IClientTransport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MoongTangE {

class GraphicsWindow;

// Composes presentation and a transport interface; no client-side physics ownership.
class ClientApplication
{
  public:
    explicit ClientApplication(ClientOptions options);

    void Run();

  private:
    void StartGame(MainMenuAction action);
    void ReturnToMenu(std::string errorMessage);
    void CloseTransport();
    void ConfirmDisplaySettings();
    void RevertDisplaySettings(GraphicsWindow& window);
    bool SendInput(bool movementEnabled);
    void ReceiveSnapshot();

    void ObserveSmokeProgress();
    void ValidateSmokeProgress() const;

    ClientOptions m_options;
    std::unique_ptr<IClientTransport> m_transport;

    SnapshotInterpolator m_presentation;
    DisplaySettings m_displaySettings;
    DisplaySettingsStore m_displaySettingsStore;

    WorldSnapshot m_authoritativeState;
    std::optional<double> m_initialX;
    std::optional<double> m_displayConfirmationDeadline;
    std::uint64_t m_sequence{};
    std::uint64_t m_roomId{};
    std::string m_nickname;
    std::string m_errorMessage;

    bool m_optionsOpen{};
    bool m_exitMenuOpen{};
    bool m_moved{};
    bool m_rolled{};
};

} // namespace MoongTangE
