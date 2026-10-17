#pragma once
#include "Common/Physics/RigidBody.h"

namespace MoongTangE {
inline void resolve_ground(RigidBody& body)
{
    if (body.position.y <= body.radius) {
        body.position.y = body.radius;
        if (body.velocity.y < 0) {
            body.velocity.y = 0;
        }
    }
}
} // namespace MoongTangE
