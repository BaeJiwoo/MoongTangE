#pragma once
#include "Common/Math/Vector3.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <array>
#include <cstdint>
#include <vector>

namespace MoongTangE {
class SceneRenderer {
public:
    SceneRenderer();
    void draw(const WorldSnapshot& state, std::uint32_t playerId, const char* mode,
              const char* status) const;

private:
    struct BallTriangle {
        std::array<Vector3, 3> vertices;
        bool light{};
    };

    static std::vector<BallTriangle> makeBallGeometry();
    void drawBall(const PlayerState& player, bool own) const;

    std::vector<BallTriangle> ballGeometry_;
};
} // namespace MoongTangE