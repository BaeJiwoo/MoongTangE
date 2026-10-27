#pragma once

#include "Common/Math/Quaternion.h"
#include "Common/Physics/CollectibleProxy.h"
#include "Common/SimulationConfig.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace MoongTangE {

struct LocalSphereProxy
{
    Vector3 center{};
    double radius{};
};

struct SweptSphereContact
{
    double fraction{1};
    std::size_t colliderSphere{};
    Vector3 normal{0, 1, 0};
};

// Exact bounded union of local sphere proxies used for pickup, terrain contact and debug display.
class CompoundBallCollider
{
  public:
    explicit CompoundBallCollider(double coreRadius)
    {
        AddSphere({}, coreRadius);
    }

    void AddSphere(Vector3 localCenter, double radius)
    {
        if (!localCenter.Finite() || !std::isfinite(radius) || radius <= 0) {
            throw std::invalid_argument("Invalid compound sphere");
        }
        if (m_spheres.size() >= MAX_SPHERES) {
            throw std::length_error("Too many compound spheres");
        }
        m_spheres.push_back({localCenter, radius});
    }

    void AddCollectible(Vector3 localCenter, Quaternion localOrientation, CollectibleKind kind,
                        double radius)
    {
        if (!localCenter.Finite() || !localOrientation.Unit() || !std::isfinite(radius) ||
            radius <= 0) {
            throw std::invalid_argument("Invalid collectible compound proxy");
        }

        const auto& shape = collectible_proxy_shape(kind);
        for (std::size_t index = 0; index < shape.count; ++index) {
            const auto& sphere = shape.spheres[index];
            AddSphere(localCenter + localOrientation.Rotate(sphere.center * radius),
                      sphere.radius * radius);
        }
    }

    const std::vector<LocalSphereProxy>& Spheres() const
    {
        return m_spheres;
    }

    Vector3 Support(const Quaternion& orientation, Vector3 worldDirection) const
    {
        const double directionLength = worldDirection.Length();
        if (!orientation.Unit() || !worldDirection.Finite() || !std::isfinite(directionLength) ||
            directionLength == 0) {
            throw std::invalid_argument("Invalid collider support direction");
        }

        const Vector3 direction = worldDirection * (1.0 / directionLength);
        Vector3 bestPoint{};
        double bestProjection = -std::numeric_limits<double>::infinity();
        for (const auto& sphere : m_spheres) {
            const Vector3 candidate = orientation.Rotate(sphere.center) + direction * sphere.radius;
            const double projection = dot(candidate, direction);
            if (projection > bestProjection) {
                bestPoint = candidate;
                bestProjection = projection;
            }
        }
        return bestPoint;
    }

    bool SweepSphere(Vector3 start, Vector3 end, double radius, SweptSphereContact& result) const
    {
        if (!start.Finite() || !end.Finite() || !std::isfinite(radius) || radius <= 0) {
            throw std::invalid_argument("Invalid swept sphere");
        }

        bool found = false;
        result = {};
        for (std::size_t index = 0; index < m_spheres.size(); ++index) {
            const auto& sphere = m_spheres[index];
            const double combinedRadius = sphere.radius + radius;
            const Vector3 relativeStart = start - sphere.center;
            const Vector3 travel = end - start;
            const double startDistanceSquared = dot(relativeStart, relativeStart);
            const double travelDistanceSquared = dot(travel, travel);
            if (!std::isfinite(startDistanceSquared) || !std::isfinite(travelDistanceSquared)) {
                throw std::invalid_argument("Swept sphere path overflow");
            }

            double fraction = 0;
            if (startDistanceSquared > combinedRadius * combinedRadius) {
                const double a = travelDistanceSquared;
                if (a == 0) {
                    continue;
                }
                const double b = 2 * dot(relativeStart, travel);
                const double c = startDistanceSquared - combinedRadius * combinedRadius;
                const double discriminant = b * b - 4 * a * c;
                if (discriminant < 0) {
                    continue;
                }
                fraction = (-b - std::sqrt(discriminant)) / (2 * a);
                if (fraction < 0 || fraction > 1) {
                    continue;
                }
            }

            if (found && fraction >= result.fraction) {
                continue;
            }

            const Vector3 hitOffset = relativeStart + travel * fraction;
            const double hitDistance = hitOffset.Length();
            result.fraction = fraction;
            result.colliderSphere = index;
            result.normal = hitDistance > MIN_DIRECTION_LENGTH ? hitOffset * (1.0 / hitDistance)
                                                               : Vector3{0, 1, 0};
            found = true;
        }
        return found;
    }

  private:
    static constexpr std::size_t MAX_SPHERES =
        1 + simulation::MAX_COLLECTIBLES * CollectibleProxyShape::MAX_SPHERES;
    static constexpr double MIN_DIRECTION_LENGTH = 1e-12;

    std::vector<LocalSphereProxy> m_spheres;
};

} // namespace MoongTangE
