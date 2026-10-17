#pragma once
#include "Common/Physics/Collision/SpherePlane.h"

namespace sandbox {
struct PhysicsMaterial {
    double friction{0.8};
    double rollingResistance{0.12};
    double groundDrag{1.8};
};

class PhysicsWorld {
    PhysicsMaterial material_;

public:
    static constexpr double fixedDelta = 1.0 / 60.0;

    explicit PhysicsWorld(PhysicsMaterial material = {}) : material_(material) {
        for (double value : {material.friction, material.rollingResistance, material.groundDrag}) {
            if (!std::isfinite(value) || value < 0) {
                throw std::invalid_argument("Invalid material");
            }
        }
    }

    void step(RigidBody& body, double dt = fixedDelta, Vector3 groundForce = {}) const {
        if (!std::isfinite(dt) || dt <= 0 || dt > 0.1 || !body.position.finite() ||
            !body.velocity.finite() || !body.angularVelocity.finite() || !body.orientation.unit() ||
            !groundForce.finite() || !std::isfinite(body.radius) || body.radius <= 0 ||
            !std::isfinite(body.mass) || body.mass <= 0 || !std::isfinite(body.inertia()) ||
            body.inertia() <= 0) {
            throw std::invalid_argument("Invalid physics state or time step");
        }
        const bool grounded = body.position.y <= body.radius + 1e-9 && body.velocity.y <= 0;
        groundForce.y = 0;
        if (grounded) {
            body.velocity = body.velocity + groundForce * (dt / body.mass);
        }
        body.velocity = body.velocity + Vector3{0, -9.81, 0} * dt;
        body.position.y += body.velocity.y * dt;
        if (body.position.y <= body.radius) {
            const double normalImpulse = body.mass * std::max(0.0, -body.velocity.y);
            resolveGround(body);
            const double damping = std::exp(-material_.groundDrag * dt / body.mass);
            body.velocity.x *= damping;
            body.velocity.z *= damping;
            // Rolling resistance is a contact torque, bounded so it cannot reverse spin.
            const Vector3 rolling{body.angularVelocity.x, 0, body.angularVelocity.z};
            const double speed = rolling.length();
            if (speed > 0) {
                const double change = material_.rollingResistance * body.mass * 9.81 * body.radius *
                                      dt / body.inertia();
                body.angularVelocity =
                    body.angularVelocity - rolling * (std::min(speed, change) / speed);
            }
            const Vector3 arm{0, -body.radius, 0};
            const Vector3 slip =
                Vector3{body.velocity.x, 0, body.velocity.z} + cross(body.angularVelocity, arm);
            // A solid sphere has I=2/5 mr^2. Equal/opposite contact impulses couple v and omega.
            Vector3 impulse =
                slip * (-1.0 / (1.0 / body.mass + body.radius * body.radius / body.inertia()));
            const double magnitude = impulse.length(), limit = material_.friction * normalImpulse;
            if (magnitude > limit) {
                impulse = impulse * (limit / magnitude);
            }
            body.velocity = body.velocity + impulse * (1.0 / body.mass);
            body.angularVelocity =
                body.angularVelocity + cross(arm, impulse) * (1.0 / body.inertia());
            if (groundForce.length() == 0 && material_.rollingResistance > 0 &&
                std::hypot(body.velocity.x, body.velocity.z) < 0.015 &&
                std::hypot(body.angularVelocity.x, body.angularVelocity.z) * body.radius < 0.015) {
                body.velocity.x = body.velocity.z = 0;
                body.angularVelocity.x = body.angularVelocity.z = 0;
            }
        }
        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;
        body.orientation =
            (Quaternion::fromRotation(body.angularVelocity * dt) * body.orientation).normalized();
    }
};
} // namespace sandbox
