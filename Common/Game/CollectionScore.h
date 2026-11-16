#pragma once

#include "Common/SimulationConfig.h"

#include <cstdint>
#include <set>

namespace MoongTangE {

// One award per object per player per round, even after defeat or stealing it back.
class CollectionScore
{
  public:
    void Collect(std::uint32_t collectibleId)
    {
        if (collectibleId != 0 && m_collected.size() < simulation::MAX_COLLECTIBLES) {
            m_collected.insert(collectibleId);
        }
    }

    std::uint32_t Points() const
    {
        return static_cast<std::uint32_t>(m_collected.size()) * simulation::COLLECTION_POINTS;
    }

  private:
    std::set<std::uint32_t> m_collected;
};

} // namespace MoongTangE
