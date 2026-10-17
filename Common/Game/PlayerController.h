#pragma once
#include "Common/Math/Vector3.h"
#include "Common/SimulationConfig.h"

namespace sandbox {
inline Vector3 movementForce(double x, double z) {
    Vector3 direction{x, 0, z};
    if (direction.length() > 1) {
        direction = direction.normalized();
    }
    return direction * simulation::movementForce;
}
} // namespace sandbox
