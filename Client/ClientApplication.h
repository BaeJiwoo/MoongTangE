#pragma once
#include "Client/ClientOptions.h"
#include "Client/Rendering/Scene.h"
#include "Client/Rendering/SnapshotInterpolator.h"
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/Transport/IClientTransport.h"
#include <cstdint>
#include <memory>

namespace sandbox {
// Composes presentation and a transport interface; no client-side physics ownership.
class ClientApplication {
public:
    explicit ClientApplication(ClientOptions options);
    void run();

private:
    void sendInput();
    void receiveSnapshot();
    void observeSmokeProgress();
    void validateSmokeProgress() const;

    ClientOptions options_;
    std::unique_ptr<IClientTransport> transport_;
    SceneRenderer renderer_;
    SnapshotInterpolator presentation_;
    WorldSnapshot authoritativeState_;
    std::uint64_t sequence_{};
    bool moved_{};
    bool rolled_{};
};
} // namespace sandbox