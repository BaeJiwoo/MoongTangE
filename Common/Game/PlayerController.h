#pragma once

#include "Common/Math/Vector3.h"
#include "Common/SimulationConfig.h"

namespace MoongTangE {

inline Vector3 movement_force(double x, double z)
{
    Vector3 direction{x, 0, z};
    if (direction.Length() > 1) {
        direction = direction.Normalized();
    }
    return direction * simulation::MOVEMENT_FORCE;
}

} // namespace MoongTangE
