#pragma once
#include "Common/Math/Vector3.h"
namespace sandbox {
inline Vector3 movementVelocity(double x, double z) {
    Vector3 direction{x,0,z};
    if (direction.length() > 1) direction = direction.normalized();
    return direction*5.0;
}
}
