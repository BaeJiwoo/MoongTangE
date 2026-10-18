#pragma once

#include "Client/ClientOptions.h"
#include "Client/Rendering/Scene.h"
#include "Client/Rendering/SnapshotInterpolator.h"
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/Transport/IClientTransport.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace MoongTangE {

// Composes presentation and a transport interface; no client-side physics ownership.
class ClientApplication
{
  public:
    explicit ClientApplication(ClientOptions options);

    void Run();

  private:
    void SendInput();
    void ReceiveSnapshot();

    void ObserveSmokeProgress();
    void ValidateSmokeProgress() const;

    ClientOptions m_options;
    std::unique_ptr<IClientTransport> m_transport;

    SceneRenderer m_renderer;
    SnapshotInterpolator m_presentation;

    WorldSnapshot m_authoritativeState;
    std::optional<double> m_initialX;
    std::uint64_t m_sequence{};
    std::uint64_t m_roomId{};

    bool m_moved{};
    bool m_rolled{};
};

} // namespace MoongTangE
