#pragma once

#include "Common/Physics/Collision/SphereTerrain.h"
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

        groundForce.y = 0;
        auto contact = MakeContact(body, collider);
        ApplyForces(body, contact, dt, groundForce);
        body.position.y += body.velocity.y * dt;

        contact = MakeContact(body, collider);
        if (contact.terrain.penetration >= 0) {
            ResolveContact(body, contact, dt, groundForce);
        }

        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;

        contact = MakeContact(body, collider);
        CorrectPenetration(body, contact.terrain);

        const auto rotation = Quaternion::FromRotation(body.angularVelocity * dt);
        body.orientation = (rotation * body.orientation).Normalized();

        contact = MakeContact(body, collider);
        CorrectPenetration(body, contact.terrain);
    }

  private:
    struct ColliderTerrainContact
    {
        TerrainContact terrain;
        Vector3 arm;
    };

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

    static ColliderTerrainContact MakeContact(const RigidBody& body,
                                              const DeformableBallCollider& collider)
    {
        const auto sample = terrain_sample(body.position.x, body.position.z);
        const Vector3 contactArm = collider.Support(body.orientation, sample.normal * -1);
        const double normalDistance = (body.position.y - sample.height) * sample.normal.y;
        const double supportDistance = -dot(contactArm, sample.normal);
        return {{sample.normal, supportDistance - normalDistance}, contactArm};
    }

    static void CorrectPenetration(RigidBody& body, TerrainContact contact)
    {
        if (contact.penetration > 0) {
            body.position = body.position + contact.normal * contact.penetration;
        }
    }

    static void ApplyForces(RigidBody& body, const ColliderTerrainContact& contact, double dt,
                            Vector3 groundForce)
    {
        const Vector3 contactVelocity = body.velocity + cross(body.angularVelocity, contact.arm);
        const bool grounded = contact.terrain.penetration >= -CONTACT_TOLERANCE &&
                              dot(contactVelocity, contact.terrain.normal) <= CONTACT_TOLERANCE;
        if (grounded) {
            groundForce =
                groundForce - contact.terrain.normal * dot(groundForce, contact.terrain.normal);
            body.velocity = body.velocity + groundForce * (dt / body.mass);
        }

        body.velocity = body.velocity + Vector3{0, -simulation::GRAVITY, 0} * dt;
    }

    void ApplyGroundDrag(RigidBody& body, double dt, Vector3 normal) const
    {
        const double damping = std::exp(-m_material.groundDrag * dt / body.mass);
        const Vector3 normalVelocity = normal * dot(body.velocity, normal);
        body.velocity = normalVelocity + (body.velocity - normalVelocity) * damping;
    }

    void ApplyRollingResistance(RigidBody& body, Vector3 contactArm, double dt,
                                Vector3 normal) const
    {
        const Vector3 rolling = body.angularVelocity - normal * dot(body.angularVelocity, normal);
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

    static double ApplyNormalImpulse(RigidBody& body, Vector3 contactArm, Vector3 normal)
    {
        const Vector3 contactVelocity = body.velocity + cross(body.angularVelocity, contactArm);
        const double normalSpeed = dot(contactVelocity, normal);
        if (normalSpeed >= 0) {
            return 0;
        }

        const Vector3 rotationalArm = cross(contactArm, normal);
        const double inverseEffectiveMass =
            1.0 / body.mass + dot(rotationalArm, rotationalArm) / body.Inertia();
        const double impulseMagnitude = -normalSpeed / inverseEffectiveMass;
        ApplyImpulse(body, contactArm, normal * impulseMagnitude);
        return impulseMagnitude;
    }

    void ApplyContactFriction(RigidBody& body, Vector3 contactArm, double normalImpulse,
                              Vector3 normal) const
    {
        const Vector3 contactVelocity = body.velocity + cross(body.angularVelocity, contactArm);
        const Vector3 slip = contactVelocity - normal * dot(contactVelocity, normal);
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

    void SleepIfResting(RigidBody& body, Vector3 contactArm, Vector3 groundForce,
                        Vector3 normal) const
    {
        const Vector3 tangentialVelocity = body.velocity - normal * dot(body.velocity, normal);
        const Vector3 contactSpin = cross(body.angularVelocity, contactArm);
        const Vector3 tangentialSpin = contactSpin - normal * dot(contactSpin, normal);
        if (groundForce.Length() == 0 && m_material.rollingResistance > 0 &&
            tangentialVelocity.Length() < SLEEP_SPEED && tangentialSpin.Length() < SLEEP_SPEED) {
            body.velocity = normal * dot(body.velocity, normal);
            body.angularVelocity = normal * dot(body.angularVelocity, normal);
        }
    }

    void ResolveContact(RigidBody& body, const ColliderTerrainContact& contact, double dt,
                        Vector3 groundForce) const
    {
        CorrectPenetration(body, contact.terrain);
        const double normalImpulse = ApplyNormalImpulse(body, contact.arm, contact.terrain.normal);
        ApplyGroundDrag(body, dt, contact.terrain.normal);
        ApplyRollingResistance(body, contact.arm, dt, contact.terrain.normal);
        ApplyContactFriction(body, contact.arm, normalImpulse, contact.terrain.normal);
        SleepIfResting(body, contact.arm, groundForce, contact.terrain.normal);
    }

    PhysicsMaterial m_material;
};

} // namespace MoongTangE
