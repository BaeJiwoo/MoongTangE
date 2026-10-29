#pragma once

#include "Client/Rendering/DisplaySettings.h"
#include "Common/Math/Vector3.h"
#include "Common/Protocol/WorldSnapshot.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MoongTangE {

enum class MenuAction
{
    NONE,
    SINGLE_PLAYER,
    MULTIPLAYER,
    RESUME_GAME,
    EXIT_GAME,
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
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    SceneRenderer(SceneRenderer&&) = delete;
    SceneRenderer& operator=(SceneRenderer&&) = delete;

    MenuAction DrawMainMenu(bool multiplayerAvailable, bool optionsOpen,
                            const DisplaySettingsState& selectedSettings, bool confirmationOpen,
                            int confirmationSeconds, std::string& nickname,
                            const std::string& errorMessage) const;
    MenuAction Draw(const WorldSnapshot& state, std::uint32_t playerId, const char* mode,
                    const char* status, bool gameMenuOpen, bool optionsOpen,
                    const DisplaySettingsState& selectedSettings, bool confirmationOpen,
                    int confirmationSeconds) const;

  private:
    struct BallTriangle
    {
        std::array<Vector3, 3> vertices;
        bool light{};
    };

    static std::vector<BallTriangle> MakeBallGeometry();

    void DrawBall(const PlayerState& player, bool own) const;
    void DrawCollectible(const CollectibleState& collectible) const;
    void DrawPhysicsDebug(const WorldSnapshot& state, const PlayerState& player) const;

    class CollectibleModels;

    std::vector<BallTriangle> m_ballGeometry;
    std::unique_ptr<CollectibleModels> m_collectibleModels;
    mutable bool m_showPhysicsDebug{};
};

} // namespace MoongTangE
