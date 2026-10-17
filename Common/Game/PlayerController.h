#pragma once
#include "Common/Math/Vector3.h"

namespace sandbox {
inline Vector3 movementForce(double x, double z) {
    Vector3 direction{x, 0, z};
    if (direction.length() > 1) {
        direction = direction.normalized();
    }
    return direction * 10.0;
}
} // namespace sandbox
