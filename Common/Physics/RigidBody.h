#pragma once
#include "Common/Math/Quaternion.h"
namespace sandbox {
struct RigidBody {
    Vector3 position{0,2,0};
    Vector3 velocity{};
    double radius{0.5};
    double mass{1};
    Vector3 angularVelocity{};
    Quaternion orientation{};
    double inertia() const { return 0.4*mass*radius*radius; }
};
}
