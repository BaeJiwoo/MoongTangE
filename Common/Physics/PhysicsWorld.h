#pragma once
#include "Common/Physics/Collision/SpherePlane.h"
#include "Common/SimulationConfig.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sandbox {
struct PhysicsMaterial {
    double friction{0.8};
    double rollingResistance{0.12};
    double groundDrag{1.8};
};

// Stateless integration of caller-owned bodies; material parameters are validated once.
class PhysicsWorld {
public:
    static constexpr double fixedDelta = simulation::FIXED_DELTA;

    explicit PhysicsWorld(PhysicsMaterial material = {}) : material_(material) {
        for (const double value :
             {material.friction, material.rollingResistance, material.groundDrag}) {
            if (!std::isfinite(value) || value < 0) {
                throw std::invalid_argument("Invalid physics material");
            }
        }
    }

    void step(RigidBody& body, double dt = fixedDelta, Vector3 groundForce = {}) const {
        validateStep(body, dt, groundForce);
        groundForce.y = 0;
        applyForces(body, dt, groundForce);
        body.position.y += body.velocity.y * dt;

        if (body.position.y <= body.radius) {
            resolveContact(body, dt, groundForce);
        }

        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;
        const auto rotation = Quaternion::fromRotation(body.angularVelocity * dt);
        body.orientation = (rotation * body.orientation).normalized();
    }

private:
    static constexpr double maximumStep = 0.1;
    static constexpr double contactTolerance = 1e-9;
    static constexpr double sleepSpeed = 0.015;

    static void validateStep(const RigidBody& body, double dt, Vector3 force) {
        if (!std::isfinite(dt) || dt <= 0 || dt > maximumStep || !body.position.finite() ||
            !body.velocity.finite() || !body.angularVelocity.finite() || !body.orientation.unit() ||
            !force.finite() || !std::isfinite(body.radius) || body.radius <= 0 ||
            !std::isfinite(body.mass) || body.mass <= 0 || !std::isfinite(body.inertia()) ||
            body.inertia() <= 0) {
            throw std::invalid_argument("Invalid physics state or time step");
        }
    }

    static void applyForces(RigidBody& body, double dt, Vector3 groundForce) {
        const bool grounded =
            body.position.y <= body.radius + contactTolerance && body.velocity.y <= 0;
        if (grounded) {
            body.velocity = body.velocity + groundForce * (dt / body.mass);
        }
        body.velocity = body.velocity + Vector3{0, -simulation::GRAVITY, 0} * dt;
    }

    void applyGroundDrag(RigidBody& body, double dt) const {
        const double damping = std::exp(-material_.groundDrag * dt / body.mass);
        body.velocity.x *= damping;
        body.velocity.z *= damping;
    }

    void applyRollingResistance(RigidBody& body, double dt) const {
        const Vector3 rolling{body.angularVelocity.x, 0, body.angularVelocity.z};
        const double speed = rolling.length();
        if (speed == 0) {
            return;
        }
        // Bound the contact torque so resistance alone cannot reverse the spin.
        const double change = material_.rollingResistance * body.mass * simulation::GRAVITY *
                              body.radius * dt / body.inertia();
        body.angularVelocity = body.angularVelocity - rolling * (std::min(speed, change) / speed);
    }

    void applyContactFriction(RigidBody& body, double normalImpulse) const {
        const Vector3 contactArm{0, -body.radius, 0};
        const Vector3 slip =
            Vector3{body.velocity.x, 0, body.velocity.z} + cross(body.angularVelocity, contactArm);
        // Solid-sphere I = 2/5 mr^2: one contact impulse changes translation and rotation.
        const double inverseEffectiveMass =
            1.0 / body.mass + body.radius * body.radius / body.inertia();
        Vector3 impulse = slip * (-1.0 / inverseEffectiveMass);
        const double magnitude = impulse.length();
        const double limit = material_.friction * normalImpulse;
        if (magnitude > limit) {
            impulse = impulse * (limit / magnitude);
        }
        body.velocity = body.velocity + impulse * (1.0 / body.mass);
        body.angularVelocity =
            body.angularVelocity + cross(contactArm, impulse) * (1.0 / body.inertia());
    }

    void sleepIfResting(RigidBody& body, Vector3 groundForce) const {
        const double linearSpeed = std::hypot(body.velocity.x, body.velocity.z);
        const double rollingSpeed =
            std::hypot(body.angularVelocity.x, body.angularVelocity.z) * body.radius;
        if (groundForce.length() == 0 && material_.rollingResistance > 0 &&
            linearSpeed < sleepSpeed && rollingSpeed < sleepSpeed) {
            body.velocity.x = 0;
            body.velocity.z = 0;
            body.angularVelocity.x = 0;
            body.angularVelocity.z = 0;
        }
    }

    void resolveContact(RigidBody& body, double dt, Vector3 groundForce) const {
        const double normalImpulse = body.mass * std::max(0.0, -body.velocity.y);
        resolveGround(body);
        applyGroundDrag(body, dt);
        applyRollingResistance(body, dt);
        applyContactFriction(body, normalImpulse);
        sleepIfResting(body, groundForce);
    }

    PhysicsMaterial material_;
};
} // namespace sandbox