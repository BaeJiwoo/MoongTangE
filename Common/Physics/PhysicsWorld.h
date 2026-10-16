#pragma once
#include "Common/Physics/Collision/SpherePlane.h"
namespace sandbox {
class PhysicsWorld {
public:
    static constexpr double fixedDelta = 1.0/60.0;
    void step(RigidBody& body, double dt = fixedDelta) const {
        if (!std::isfinite(dt) || dt <= 0 || dt > 0.1 ||
            !body.position.finite() || !body.velocity.finite() ||
            !std::isfinite(body.radius) || body.radius <= 0)
            throw std::invalid_argument("Invalid physics state or time step");
        body.velocity = body.velocity + Vector3{0,-9.81,0}*dt;
        body.position = body.position + body.velocity*dt;
        resolveGround(body);
    }
};
}
