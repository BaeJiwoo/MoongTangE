#pragma once

#include "Common/Protocol/WorldSnapshot.h"

#include <span>
#include <vector>

namespace MoongTangE {

struct DeformedBallVertex
{
    Vector3 position{};
    Vector3 normal{};
};

class BallDeformation
{
  public:
    BallDeformation(const PlayerState& player, std::span<const CollectibleState> collectibles);

    DeformedBallVertex Sample(Vector3 direction) const;

  private:
    struct Bulge
    {
        Vector3 direction{};
        double targetRadius{};
        double cutoffAlignment{};
    };

    std::vector<Bulge> m_bulges;
};

} // namespace MoongTangE
