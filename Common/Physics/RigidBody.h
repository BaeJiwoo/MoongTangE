#pragma once
#include "Common/Math/Quaternion.h"
#include "Common/SimulationConfig.h"

namespace sandbox {
struct RigidBody {
    Vector3 position{0, simulation::spawnHeight, 0};
    Vector3 velocity{};
    double radius{simulation::playerRadius};
    double mass{simulation::playerMass};
    Vector3 angularVelocity{};
    Quaternion orientation{};

    double inertia() const {
        return 0.4 * mass * radius * radius;
    }
};
} // namespace sandbox
