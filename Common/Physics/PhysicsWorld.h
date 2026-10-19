#pragma once

#include "Common/Physics/Collision/SphereTerrain.h"
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
        const auto initialContact = terrain_contact(body);
        ApplyForces(body, dt, groundForce, initialContact);
        body.position.y += body.velocity.y * dt;

        auto contact = terrain_contact(body);
        if (contact.penetration >= 0) {
            ResolveContact(body, dt, groundForce, contact);
        }

        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;

        contact = terrain_contact(body);
        if (contact.penetration > 0) {
            resolve_terrain(body, contact);
        }

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

    static void ApplyForces(RigidBody& body, double dt, Vector3 groundForce, TerrainContact contact)
    {
        const bool grounded =
            contact.penetration >= -CONTACT_TOLERANCE && dot(body.velocity, contact.normal) <= 0;
        if (grounded) {
            groundForce = groundForce - contact.normal * dot(groundForce, contact.normal);
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

    void ApplyRollingResistance(RigidBody& body, double dt, Vector3 normal) const
    {
        const Vector3 rolling = body.angularVelocity - normal * dot(body.angularVelocity, normal);
        const double speed = rolling.Length();
        if (speed == 0) {
            return;
        }

        // Bound the contact torque so resistance alone cannot reverse the spin.
        const double change = m_material.rollingResistance * body.mass * simulation::GRAVITY *
                              body.radius * dt / body.Inertia();
        body.angularVelocity = body.angularVelocity - rolling * (std::min(speed, change) / speed);
    }

    void ApplyContactFriction(RigidBody& body, double normalImpulse, Vector3 normal) const
    {
        const Vector3 contactArm = normal * -body.radius;
        Vector3 slip = body.velocity + cross(body.angularVelocity, contactArm);
        slip = slip - normal * dot(slip, normal);

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

    void SleepIfResting(RigidBody& body, Vector3 groundForce, Vector3 normal) const
    {
        const Vector3 tangentialVelocity = body.velocity - normal * dot(body.velocity, normal);
        const Vector3 rollingVelocity =
            body.angularVelocity - normal * dot(body.angularVelocity, normal);
        const double linearSpeed = tangentialVelocity.Length();
        const double rollingSpeed = rollingVelocity.Length() * body.radius;
        if (groundForce.Length() == 0 && m_material.rollingResistance > 0 &&
            linearSpeed < SLEEP_SPEED && rollingSpeed < SLEEP_SPEED) {
            body.velocity = normal * dot(body.velocity, normal);
            body.angularVelocity = normal * dot(body.angularVelocity, normal);
        }
    }

    void ResolveContact(RigidBody& body, double dt, Vector3 groundForce,
                        TerrainContact contact) const
    {
        const double inwardSpeed = std::min(0.0, dot(body.velocity, contact.normal));
        const double normalImpulse = body.mass * -inwardSpeed;
        resolve_terrain(body, contact);
        ApplyGroundDrag(body, dt, contact.normal);
        ApplyRollingResistance(body, dt, contact.normal);
        ApplyContactFriction(body, normalImpulse, contact.normal);

        SleepIfResting(body, groundForce, contact.normal);
    }

    PhysicsMaterial m_material;
};

} // namespace MoongTangE
