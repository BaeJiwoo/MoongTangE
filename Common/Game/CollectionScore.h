#pragma once

#include "Common/SimulationConfig.h"

#include <algorithm>
#include <cstdint>

namespace MoongTangE {

// Observe the world's cumulative consumption count; repeated snapshots cannot award twice.
class CollectionScore
{
  public:
    void Observe(std::uint32_t foodEaten)
    {
        m_foodEaten = std::max(m_foodEaten, std::min(foodEaten, simulation::MAX_FOOD_PER_ROUND));
    }

    std::uint32_t Points() const
    {
        return m_foodEaten * simulation::COLLECTION_POINTS;
    }

  private:
    std::uint32_t m_foodEaten{};
};

} // namespace MoongTangE
