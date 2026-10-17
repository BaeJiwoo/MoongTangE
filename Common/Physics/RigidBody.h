#pragma once
#include "Common/Math/Vector3.h"
namespace sandbox {
struct RigidBody {
    Vector3 position{0,2,0};
    Vector3 velocity{};
    double radius{0.5};
};
}
