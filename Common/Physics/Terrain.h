#pragma once

#include "Common/Math/Vector3.h"

#include <array>
#include <cmath>

namespace MoongTangE {

struct PointedTerrainFeature
{
    double x{};
    double z{};
    double radius{};
    double height{};
};

struct HillTerrainFeature
{
    double x{};
    double z{};
    double radius{};
    double centerY{};
};

struct TerrainSample
{
    double height{};
    Vector3 normal{0, 1, 0};
};

inline constexpr PointedTerrainFeature POINTED_TERRAIN{5.0, 4.0, 1.8, 2.2};
inline constexpr std::array<HillTerrainFeature, 2> HILL_TERRAIN{{
    {-4.0, 1.0, 3.0, -1.8},
    {1.0, -5.0, 3.5, -2.4},
}};

inline TerrainSample terrain_sample(double x, double z)
{
    TerrainSample result;

    const double pointedX = x - POINTED_TERRAIN.x;
    const double pointedZ = z - POINTED_TERRAIN.z;
    const double pointedDistance = std::hypot(pointedX, pointedZ);
    if (pointedDistance < POINTED_TERRAIN.radius) {
        const double height =
            POINTED_TERRAIN.height * (1.0 - pointedDistance / POINTED_TERRAIN.radius);
        const Vector3 normal = pointedDistance > 0
                                   ? Vector3{POINTED_TERRAIN.height * pointedX /
                                                 (POINTED_TERRAIN.radius * pointedDistance),
                                             1,
                                             POINTED_TERRAIN.height * pointedZ /
                                                 (POINTED_TERRAIN.radius * pointedDistance)}
                                         .Normalized()
                                   : Vector3{0, 1, 0};
        result = {height, normal};
    }

    for (const auto& hill : HILL_TERRAIN) {
        const double hillX = x - hill.x;
        const double hillZ = z - hill.z;
        const double radialSquared = hillX * hillX + hillZ * hillZ;
        if (radialSquared >= hill.radius * hill.radius) {
            continue;
        }

        const double verticalRadius = std::sqrt(hill.radius * hill.radius - radialSquared);
        const double height = hill.centerY + verticalRadius;
        if (height > result.height) {
            result = {height, Vector3{hillX, verticalRadius, hillZ}.Normalized()};
        }
    }
    return result;
}

} // namespace MoongTangE
