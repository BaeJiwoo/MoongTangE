#pragma once

#include "Common/Math/Vector3.h"
#include "Common/Protocol/WorldSnapshot.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace MoongTangE {

enum class MainMenuAction
{
    NONE,
    SINGLE_PLAYER,
    MULTIPLAYER,
    DISMISS_ERROR
};

class SceneRenderer
{
  public:
    SceneRenderer();

    MainMenuAction DrawMainMenu(bool multiplayerAvailable, std::string& nickname,
                                const std::string& errorMessage) const;
    void Draw(const WorldSnapshot& state, std::uint32_t playerId, const char* mode,
              const char* status) const;

  private:
    struct BallTriangle
    {
        std::array<Vector3, 3> vertices;
        bool light{};
    };

    static std::vector<BallTriangle> MakeBallGeometry();

    void DrawBall(const PlayerState& player, bool own) const;

    std::vector<BallTriangle> m_ballGeometry;
};

} // namespace MoongTangE
