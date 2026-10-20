#pragma once

#include "Common/Physics/RigidBody.h"
#include "Common/Physics/Terrain.h"

namespace MoongTangE {

struct TerrainContact
{
    Vector3 normal{0, 1, 0};
    double penetration{};
};

inline TerrainContact terrain_contact(const RigidBody& body)
{
    const auto terrain = terrain_sample(body.position.x, body.position.z);
    const double normalDistance = (body.position.y - terrain.height) * terrain.normal.y;
    return {terrain.normal, body.radius - normalDistance};
}

inline void resolve_terrain(RigidBody& body, TerrainContact contact)
{
    if (contact.penetration < 0) {
        return;
    }

    body.position = body.position + contact.normal * contact.penetration;
    const double normalSpeed = dot(body.velocity, contact.normal);
    if (normalSpeed < 0) {
        body.velocity = body.velocity - contact.normal * normalSpeed;
    }
}

} // namespace MoongTangE
