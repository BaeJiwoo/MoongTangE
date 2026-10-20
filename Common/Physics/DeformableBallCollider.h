#pragma once

#include "Common/Math/Quaternion.h"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace MoongTangE {

struct ColliderSphere
{
    Vector3 localCenter{};
    double radius{};
};

// The convex support of a core ball and its attached spherical collectibles.
class DeformableBallCollider
{
  public:
    explicit DeformableBallCollider(double coreRadius)
    {
        AddSphere({}, coreRadius);
    }

    void AddSphere(Vector3 localCenter, double radius)
    {
        if (!localCenter.Finite() || !std::isfinite(radius) || radius <= 0) {
            throw std::invalid_argument("Invalid collider sphere");
        }
        m_spheres.push_back({localCenter, radius});
    }

    Vector3 Support(const Quaternion& orientation, Vector3 worldDirection) const
    {
        const double directionLength = worldDirection.Length();
        if (!orientation.Unit() || !worldDirection.Finite() || !std::isfinite(directionLength) ||
            directionLength == 0) {
            throw std::invalid_argument("Invalid collider support direction");
        }

        worldDirection = worldDirection * (1.0 / directionLength);
        Vector3 bestPoint{};
        double bestProjection = 0;
        bool found = false;

        for (const auto& sphere : m_spheres) {
            const Vector3 worldCenter = orientation.Rotate(sphere.localCenter);
            const Vector3 candidate = worldCenter + worldDirection * sphere.radius;
            const double projection = dot(candidate, worldDirection);
            if (!found || projection > bestProjection) {
                bestPoint = candidate;
                bestProjection = projection;
                found = true;
            }
        }
        return bestPoint;
    }

    std::optional<ColliderSphere> FindOverlap(Vector3 localCenter, double radius) const
    {
        if (!localCenter.Finite() || !std::isfinite(radius) || radius <= 0) {
            throw std::invalid_argument("Invalid overlap sphere");
        }

        for (const auto& sphere : m_spheres) {
            const Vector3 offset = localCenter - sphere.localCenter;
            const double combinedRadius = radius + sphere.radius;
            if (dot(offset, offset) <= combinedRadius * combinedRadius) {
                return sphere;
            }
        }
        return std::nullopt;
    }

  private:
    std::vector<ColliderSphere> m_spheres;
};

} // namespace MoongTangE
