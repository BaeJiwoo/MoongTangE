#pragma once

#include "Common/Physics/Collision/SphereTerrain.h"
#include "Common/Physics/CompoundBallCollider.h"
#include "Common/Physics/RigidBody.h"
#include "Common/SimulationConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace MoongTangE {

struct PhysicsMaterial
{
    double friction{0.8};
    double rollingResistance{0.12};
    double groundDrag{1.8};
};

struct CompoundTerrainContact
{
    TerrainContact terrain;
    Vector3 arm;
    Vector3 point;
    std::size_t sphereIndex{};
};

struct CompoundTerrainContacts
{
    static constexpr std::size_t MAX_CONTACTS = 4;

    std::array<CompoundTerrainContact, MAX_CONTACTS> points{};
    std::size_t count{};
};

// Stateless integration of caller-owned bodies; material parameters are validated once.
class PhysicsWorld
{
  public:
    static constexpr double FIXED_DELTA = simulation::FIXED_DELTA;
    static constexpr double CONTACT_TOLERANCE = 1e-9;

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
        Step(body, CompoundBallCollider(body.radius), dt, groundForce);
    }

    void Step(RigidBody& body, const CompoundBallCollider& collider, double dt = FIXED_DELTA,
              Vector3 groundForce = {}) const
    {
        ValidateStep(body, dt, groundForce);

        groundForce.y = 0;
        auto contacts = TerrainContacts(body, collider);
        ApplyForces(body, contacts, dt, groundForce);
        body.position.y += body.velocity.y * dt;

        ResolveContacts(body, collider, dt, groundForce);

        body.position.x += body.velocity.x * dt;
        body.position.z += body.velocity.z * dt;
        CorrectPenetration(body, collider);

        const auto rotation = Quaternion::FromRotation(body.angularVelocity * dt);
        body.orientation = (rotation * body.orientation).Normalized();
        CorrectPenetration(body, collider);
    }

    void ConstrainToTerrain(RigidBody& body) const
    {
        ConstrainToTerrain(body, CompoundBallCollider(body.radius));
    }

    void ConstrainToTerrain(RigidBody& body, const CompoundBallCollider& collider) const
    {
        CorrectPenetration(body, collider);
        const auto contacts = TerrainContacts(body, collider);
        for (std::size_t index = 0; index < contacts.count; ++index) {
            const auto& contact = contacts.points[index];
            if (contact.terrain.penetration < -CONTACT_TOLERANCE) {
                break;
            }
            ApplyNormalImpulse(body, contact.arm, contact.terrain.normal);
        }
    }

    static CompoundTerrainContacts TerrainContacts(const RigidBody& body,
                                                   const CompoundBallCollider& collider)
    {
        CompoundTerrainContacts result;
        for (std::size_t sphereIndex = 0; sphereIndex < collider.Spheres().size(); ++sphereIndex) {
            const auto& sphere = collider.Spheres()[sphereIndex];
            const Vector3 centerOffset = body.orientation.Rotate(sphere.center);
            const Vector3 center = body.position + centerOffset;
            const auto sample = terrain_sample(center.x, center.z);
            const double normalDistance = (center.y - sample.height) * sample.normal.y;
            const double penetration = sphere.radius - normalDistance;
            const Vector3 arm = centerOffset - sample.normal * sphere.radius;
            const CompoundTerrainContact candidate{
                {sample.normal, penetration}, arm, body.position + arm, sphereIndex};

            std::size_t insertion = 0;
            while (insertion < result.count &&
                   result.points[insertion].terrain.penetration >= penetration) {
                ++insertion;
            }
            if (insertion >= CompoundTerrainContacts::MAX_CONTACTS) {
                continue;
            }

            result.count = std::min(result.count + 1, CompoundTerrainContacts::MAX_CONTACTS);
            for (std::size_t move = result.count - 1; move > insertion; --move) {
                result.points[move] = result.points[move - 1];
            }
            result.points[insertion] = candidate;
        }
        return result;
    }

  private:
    static constexpr double MAX_STEP = 0.1;
    static constexpr double SLEEP_SPEED = 0.015;
    static constexpr int POSITION_SOLVER_ITERATIONS = 4;

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

    static void CorrectPenetration(RigidBody& body, const CompoundBallCollider& collider)
    {
        for (int iteration = 0; iteration < POSITION_SOLVER_ITERATIONS; ++iteration) {
            const auto contacts = TerrainContacts(body, collider);
            if (contacts.count == 0 || contacts.points[0].terrain.penetration <= 0) {
                return;
            }

            std::size_t penetratingContacts = 0;
            while (penetratingContacts < contacts.count &&
                   contacts.points[penetratingContacts].terrain.penetration > 0) {
                ++penetratingContacts;
            }
            for (std::size_t index = 0; index < penetratingContacts; ++index) {
                const auto& contact = contacts.points[index].terrain;
                const double correction =
                    contact.penetration / static_cast<double>(penetratingContacts);
                body.position = body.position + contact.normal * correction;
            }
        }
    }

    static void ApplyForces(RigidBody& body, const CompoundTerrainContacts& contacts, double dt,
                            Vector3 groundForce)
    {
        bool grounded = false;
        Vector3 groundNormal{0, 1, 0};
        if (contacts.count > 0) {
            const auto& contact = contacts.points[0];
            const Vector3 contactVelocity =
                body.velocity + cross(body.angularVelocity, contact.arm);
            grounded = contact.terrain.penetration >= -CONTACT_TOLERANCE &&
                       dot(contactVelocity, contact.terrain.normal) <= CONTACT_TOLERANCE;
            groundNormal = contact.terrain.normal;
        }
        if (grounded) {
            groundForce = groundForce - groundNormal * dot(groundForce, groundNormal);
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

    void ApplyRollingResistance(RigidBody& body, Vector3 contactArm, double dt) const
    {
        const double speed = body.angularVelocity.Length();
        if (speed == 0) {
            return;
        }

        const double change = m_material.rollingResistance * body.mass * simulation::GRAVITY *
                              contactArm.Length() * dt / body.Inertia();
        body.angularVelocity = body.angularVelocity * (1 - std::min(speed, change) / speed);
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

    void SleepIfResting(RigidBody& body, const CompoundTerrainContacts& contacts,
                        Vector3 groundForce) const
    {
        if (groundForce.Length() != 0 || m_material.rollingResistance <= 0) {
            return;
        }

        for (std::size_t index = 0; index < contacts.count; ++index) {
            const auto& contact = contacts.points[index];
            if (contact.terrain.penetration < -CONTACT_TOLERANCE) {
                break;
            }
            const Vector3 contactVelocity =
                body.velocity + cross(body.angularVelocity, contact.arm);
            const Vector3 tangent =
                contactVelocity -
                contact.terrain.normal * dot(contactVelocity, contact.terrain.normal);
            if (tangent.Length() >= SLEEP_SPEED) {
                return;
            }
        }

        if (body.velocity.Length() < SLEEP_SPEED && body.angularVelocity.Length() < SLEEP_SPEED) {
            body.velocity = {};
            body.angularVelocity = {};
        }
    }

    void ResolveContacts(RigidBody& body, const CompoundBallCollider& collider, double dt,
                         Vector3 groundForce) const
    {
        CorrectPenetration(body, collider);
        const auto contacts = TerrainContacts(body, collider);
        if (contacts.count == 0 || contacts.points[0].terrain.penetration < -CONTACT_TOLERANCE) {
            return;
        }

        std::array<double, CompoundTerrainContacts::MAX_CONTACTS> normalImpulses{};
        std::size_t activeContacts = 0;
        for (std::size_t index = 0; index < contacts.count; ++index) {
            const auto& contact = contacts.points[index];
            if (contact.terrain.penetration < -CONTACT_TOLERANCE) {
                break;
            }
            normalImpulses[index] = ApplyNormalImpulse(body, contact.arm, contact.terrain.normal);
            ++activeContacts;
        }

        const auto& deepest = contacts.points[0];
        ApplyGroundDrag(body, dt, deepest.terrain.normal);
        ApplyRollingResistance(body, deepest.arm, dt);
        for (std::size_t index = 0; index < activeContacts; ++index) {
            const auto& contact = contacts.points[index];
            ApplyContactFriction(body, contact.arm, normalImpulses[index], contact.terrain.normal);
        }
        SleepIfResting(body, contacts, groundForce);
    }

    PhysicsMaterial m_material;
};

} // namespace MoongTangE
