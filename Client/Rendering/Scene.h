#pragma once

#include "Client/Rendering/DisplaySettings.h"
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
    SHOW_OPTIONS,
    PREVIOUS_WINDOW_MODE,
    NEXT_WINDOW_MODE,
    PREVIOUS_MONITOR,
    NEXT_MONITOR,
    PREVIOUS_RESOLUTION,
    NEXT_RESOLUTION,
    TOGGLE_VSYNC,
    PREVIOUS_FRAME_RATE_LIMIT,
    NEXT_FRAME_RATE_LIMIT,
    APPLY_OPTIONS,
    CANCEL_OPTIONS,
    CONFIRM_DISPLAY_SETTINGS,
    REVERT_DISPLAY_SETTINGS,
    DISMISS_ERROR
};

class SceneRenderer
{
  public:
    SceneRenderer();

    MainMenuAction DrawMainMenu(bool multiplayerAvailable, bool optionsOpen,
                                const DisplaySettingsState& selectedSettings, bool confirmationOpen,
                                int confirmationSeconds, std::string& nickname,
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
