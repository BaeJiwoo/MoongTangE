#pragma once
#include "Common/Math/Quaternion.h"
#include "Common/SimulationConfig.h"

namespace sandbox {
struct RigidBody {
    Vector3 position{0, simulation::SPAWN_HEIGHT, 0};
    Vector3 velocity{};
    double radius{simulation::PLAYER_RADIUS};
    double mass{simulation::PLAYER_MASS};
    Vector3 angularVelocity{};
    Quaternion orientation{};

    double inertia() const {
        return 0.4 * mass * radius * radius;
    }
};
} // namespace sandbox
