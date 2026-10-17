#pragma once

#include "Common/Physics/Collision/SpherePlane.h"
#include "Common/SimulationConfig.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MoongTangE {

struct PhysicsMaterial
{
    double friction{0.8};
    double rollingResistance{0.12};
    double groundDrag{1.8};
};

// Stateless integration of caller-owned bodies; material parameters are validated once.
class PhysicsWorld
{
  public:
    static constexpr double FIXED_DELTA = simulation::FIXED_DELTA;

    explicit PhysicsWorld(PhysicsMaterial material = {}) : m_material(material)
    {
        for (const double value :
             {material.friction, material.rollingResistance, material.groundDrag}) {
            if (!std::isfinite(value) || value < 0) {
                throw std::invalid_argument("Invalid physics material");
            }
        }
    }

    void Step(RigidBody& body, double dt = FIXED_DELTA, Vector3 groundForce = {}) const
    {
        ValidateStep(body, dt, groundForce);

        groundForce.y = 0;
        ApplyForces(body, dt, groundForce);
        body.position.y += body.velocity.y * dt;

        if (body.position.y <= body.radius) {
            ResolveContact(body, dt, groundForce);
        }

        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;

        const auto rotation = Quaternion::FromRotation(body.angularVelocity * dt);
        body.orientation = (rotation * body.orientation).Normalized();
    }

  private:
    static constexpr double MAX_STEP = 0.1;
    static constexpr double CONTACT_TOLERANCE = 1e-9;
    static constexpr double SLEEP_SPEED = 0.015;

    static void ValidateStep(const RigidBody& body, double dt, Vector3 force)
    {
        if (!std::isfinite(dt) || dt <= 0 || dt > MAX_STEP || !body.position.Finite() ||
            !body.velocity.Finite() || !body.angularVelocity.Finite() || !body.orientation.Unit() ||
            !force.Finite() || !std::isfinite(body.radius) || body.radius <= 0 ||
            !std::isfinite(body.mass) || body.mass <= 0 || !std::isfinite(body.Inertia()) ||
            body.Inertia() <= 0) {
            throw std::invalid_argument("Invalid physics state or time step");
        }
    }

    static void ApplyForces(RigidBody& body, double dt, Vector3 groundForce)
    {
        const bool grounded =
            body.position.y <= body.radius + CONTACT_TOLERANCE && body.velocity.y <= 0;
        if (grounded) {
            body.velocity = body.velocity + groundForce * (dt / body.mass);
        }

        body.velocity = body.velocity + Vector3{0, -simulation::GRAVITY, 0} * dt;
    }

    void ApplyGroundDrag(RigidBody& body, double dt) const
    {
        const double damping = std::exp(-m_material.groundDrag * dt / body.mass);
        body.velocity.x *= damping;
        body.velocity.z *= damping;
    }

    void ApplyRollingResistance(RigidBody& body, double dt) const
    {
        const Vector3 rolling{body.angularVelocity.x, 0, body.angularVelocity.z};
        const double speed = rolling.Length();
        if (speed == 0) {
            return;
        }

        // Bound the contact torque so resistance alone cannot reverse the spin.
        const double change = m_material.rollingResistance * body.mass * simulation::GRAVITY *
                              body.radius * dt / body.Inertia();
        body.angularVelocity = body.angularVelocity - rolling * (std::min(speed, change) / speed);
    }

    void ApplyContactFriction(RigidBody& body, double normalImpulse) const
    {
        const Vector3 contactArm{0, -body.radius, 0};
        const Vector3 slip =
            Vector3{body.velocity.x, 0, body.velocity.z} + cross(body.angularVelocity, contactArm);

        // Solid-sphere I = 2/5 mr^2: one contact impulse changes translation and rotation.
        const double inverseEffectiveMass =
            1.0 / body.mass + body.radius * body.radius / body.Inertia();
        Vector3 impulse = slip * (-1.0 / inverseEffectiveMass);
        const double magnitude = impulse.Length();
        const double limit = m_material.friction * normalImpulse;
        if (magnitude > limit) {
            impulse = impulse * (limit / magnitude);
        }

        body.velocity = body.velocity + impulse * (1.0 / body.mass);
        body.angularVelocity =
            body.angularVelocity + cross(contactArm, impulse) * (1.0 / body.Inertia());
    }

    void SleepIfResting(RigidBody& body, Vector3 groundForce) const
    {
        const double linearSpeed = std::hypot(body.velocity.x, body.velocity.z);
        const double rollingSpeed =
            std::hypot(body.angularVelocity.x, body.angularVelocity.z) * body.radius;
        if (groundForce.Length() == 0 && m_material.rollingResistance > 0 &&
            linearSpeed < SLEEP_SPEED && rollingSpeed < SLEEP_SPEED) {
            body.velocity.x = 0;
            body.velocity.z = 0;
            body.angularVelocity.x = 0;
            body.angularVelocity.z = 0;
        }
    }

    void ResolveContact(RigidBody& body, double dt, Vector3 groundForce) const
    {
        const double normalImpulse = body.mass * std::max(0.0, -body.velocity.y);
        resolve_ground(body);
        ApplyGroundDrag(body, dt);
        ApplyRollingResistance(body, dt);
        ApplyContactFriction(body, normalImpulse);

        SleepIfResting(body, groundForce);
    }

    PhysicsMaterial m_material;
};

} // namespace MoongTangE