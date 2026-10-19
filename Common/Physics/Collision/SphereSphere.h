#pragma once

#include "Common/Physics/RigidBody.h"

#include <cmath>

namespace MoongTangE {

inline constexpr double PLAYER_COLLISION_RESTITUTION = 0.15;

inline bool resolve_sphere_collision(RigidBody& first, RigidBody& second)
{
    const Vector3 separation = second.position - first.position;
    const double distanceSquared = dot(separation, separation);
    const double minimumDistance = first.radius + second.radius;
    if (distanceSquared > minimumDistance * minimumDistance) {
        return false;
    }

    const double distance = std::sqrt(distanceSquared);
    const Vector3 normal = distance > 0 ? separation * (1.0 / distance) : Vector3{1, 0, 0};
    const double inverseFirstMass = 1.0 / first.mass;
    const double inverseSecondMass = 1.0 / second.mass;
    const double inverseMass = inverseFirstMass + inverseSecondMass;

    const double penetration = minimumDistance - distance;
    if (penetration > 0) {
        first.position = first.position - normal * (penetration * inverseFirstMass / inverseMass);
        second.position =
            second.position + normal * (penetration * inverseSecondMass / inverseMass);
    }

    const double approachingSpeed = dot(second.velocity - first.velocity, normal);
    if (approachingSpeed >= 0) {
        return true;
    }

    const double impulseMagnitude =
        -(1.0 + PLAYER_COLLISION_RESTITUTION) * approachingSpeed / inverseMass;
    const Vector3 impulse = normal * impulseMagnitude;
    first.velocity = first.velocity - impulse * inverseFirstMass;
    second.velocity = second.velocity + impulse * inverseSecondMass;
    return true;
}

} // namespace MoongTangE
