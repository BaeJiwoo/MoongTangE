#pragma once

#include "Common/Physics/RigidBody.h"

namespace MoongTangE {

inline bool spheres_overlap(const RigidBody& first, const RigidBody& second)
{
    const Vector3 offset = first.position - second.position;
    const double combinedRadius = first.radius + second.radius;
    return dot(offset, offset) <= combinedRadius * combinedRadius;
}

} // namespace MoongTangE
