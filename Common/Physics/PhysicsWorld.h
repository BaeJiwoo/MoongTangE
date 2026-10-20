#pragma once

#include "Common/Physics/DeformableBallCollider.h"
#include "Common/Physics/RigidBody.h"
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
        Step(body, DeformableBallCollider(body.radius), dt, groundForce);
    }

    void Step(RigidBody& body, const DeformableBallCollider& collider, double dt = FIXED_DELTA,
              Vector3 groundForce = {}) const
    {
        ValidateStep(body, dt, groundForce);

        const Vector3 contactArm = collider.Support(body.orientation, {0, -1, 0});
        groundForce.y = 0;
        ApplyForces(body, contactArm, dt, groundForce);
        body.position.y += body.velocity.y * dt;

        if (body.position.y + contactArm.y <= 0) {
            ResolveContact(body, contactArm, dt, groundForce);
        }

        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;

        const auto rotation = Quaternion::FromRotation(body.angularVelocity * dt);
        body.orientation = (rotation * body.orientation).Normalized();

        const Vector3 updatedContactArm = collider.Support(body.orientation, {0, -1, 0});
        if (body.position.y + updatedContactArm.y < 0) {
            body.position.y = -updatedContactArm.y;
        }
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

    static void ApplyForces(RigidBody& body, Vector3 contactArm, double dt, Vector3 groundForce)
    {
        const Vector3 contactVelocity = body.velocity + cross(body.angularVelocity, contactArm);
        const bool grounded =
            body.position.y + contactArm.y <= CONTACT_TOLERANCE && contactVelocity.y <= 0;
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

    void ApplyRollingResistance(RigidBody& body, Vector3 contactArm, double dt) const
    {
        const Vector3 rolling{body.angularVelocity.x, 0, body.angularVelocity.z};
        const double speed = rolling.Length();
        if (speed == 0) {
            return;
        }

        // Bound the contact torque so resistance alone cannot reverse the spin.
        const double change = m_material.rollingResistance * body.mass * simulation::GRAVITY *
                              contactArm.Length() * dt / body.Inertia();
        body.angularVelocity = body.angularVelocity - rolling * (std::min(speed, change) / speed);
    }

    static void ApplyImpulse(RigidBody& body, Vector3 contactArm, Vector3 impulse)
    {
        body.velocity = body.velocity + impulse * (1.0 / body.mass);
        body.angularVelocity =
            body.angularVelocity + cross(contactArm, impulse) * (1.0 / body.Inertia());
    }

    static double ApplyNormalImpulse(RigidBody& body, Vector3 contactArm)
    {
        constexpr Vector3 GROUND_NORMAL{0, 1, 0};
        const Vector3 contactVelocity = body.velocity + cross(body.angularVelocity, contactArm);
        const double normalSpeed = dot(contactVelocity, GROUND_NORMAL);
        if (normalSpeed >= 0) {
            return 0;
        }

        const Vector3 rotationalArm = cross(contactArm, GROUND_NORMAL);
        const double inverseEffectiveMass =
            1.0 / body.mass + dot(rotationalArm, rotationalArm) / body.Inertia();
        const double impulseMagnitude = -normalSpeed / inverseEffectiveMass;
        ApplyImpulse(body, contactArm, GROUND_NORMAL * impulseMagnitude);
        return impulseMagnitude;
    }

    void ApplyContactFriction(RigidBody& body, Vector3 contactArm, double normalImpulse) const
    {
        const Vector3 contactVelocity = body.velocity + cross(body.angularVelocity, contactArm);
        const Vector3 slip{contactVelocity.x, 0, contactVelocity.z};
        const double slipSpeed = slip.Length();
        if (slipSpeed == 0) {
            return;
        }

        const Vector3 slipDirection = slip * (1.0 / slipSpeed);
        const Vector3 rotationalArm = cross(contactArm, slipDirection);
        const double inverseEffectiveMass =
            1.0 / body.mass + dot(rotationalArm, rotationalArm) / body.Inertia();
        double impulseMagnitude = slipSpeed / inverseEffectiveMass;
        const double limit = m_material.friction * normalImpulse;
        impulseMagnitude = std::min(impulseMagnitude, limit);

        ApplyImpulse(body, contactArm, slipDirection * -impulseMagnitude);
    }

    void SleepIfResting(RigidBody& body, Vector3 contactArm, Vector3 groundForce) const
    {
        const double linearSpeed = std::hypot(body.velocity.x, body.velocity.z);
        const Vector3 contactSpin = cross(body.angularVelocity, contactArm);
        const double rollingSpeed = std::hypot(contactSpin.x, contactSpin.z);
        if (groundForce.Length() == 0 && m_material.rollingResistance > 0 &&
            linearSpeed < SLEEP_SPEED && rollingSpeed < SLEEP_SPEED) {
            body.velocity.x = 0;
            body.velocity.z = 0;
            body.angularVelocity.x = 0;
            body.angularVelocity.z = 0;
        }
    }

    void ResolveContact(RigidBody& body, Vector3 contactArm, double dt, Vector3 groundForce) const
    {
        body.position.y = -contactArm.y;
        const double normalImpulse = ApplyNormalImpulse(body, contactArm);
        ApplyGroundDrag(body, dt);
        ApplyRollingResistance(body, contactArm, dt);
        ApplyContactFriction(body, contactArm, normalImpulse);

        SleepIfResting(body, contactArm, groundForce);
    }

    PhysicsMaterial m_material;
};

} // namespace MoongTangE
