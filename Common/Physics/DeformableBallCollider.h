#pragma once

#include "Common/Math/Quaternion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace MoongTangE {

struct RadialColliderArm
{
    Vector3 direction{};
    double length{};
};

// Separate fixed-cost proxies for smooth collection and irregular rolling.
class DeformableBallCollider
{
  public:
    static constexpr std::size_t RADIAL_ARM_COUNT = 54;

    explicit DeformableBallCollider(double coreRadius) : m_coreRadius(coreRadius)
    {
        if (!std::isfinite(coreRadius) || coreRadius <= 0) {
            throw std::invalid_argument("Invalid core radius");
        }

        m_volumeRadiusCubed = coreRadius * coreRadius * coreRadius;
        InitializeArms();
    }

    void AddCollectible(Vector3 localCenter, double radius)
    {
        if (!localCenter.Finite() || !std::isfinite(radius) || radius <= 0) {
            throw std::invalid_argument("Invalid collectible collider");
        }

        const double radiusCubed = radius * radius * radius;
        const double updatedVolumeRadiusCubed = m_volumeRadiusCubed + radiusCubed;
        if (!std::isfinite(updatedVolumeRadiusCubed)) {
            throw std::invalid_argument("Collectible collider volume overflow");
        }

        const double centerDistance = localCenter.Length();
        if (!std::isfinite(centerDistance)) {
            throw std::invalid_argument("Collectible collider center overflow");
        }

        const Vector3 outwardDirection = centerDistance > MIN_DIRECTION_LENGTH
                                             ? localCenter * (1.0 / centerDistance)
                                             : Vector3{0, 1, 0};
        auto closestArm = std::max_element(m_arms.begin(), m_arms.end(),
                                           [&](const auto& first, const auto& second) {
                                               return dot(first.direction, outwardDirection) <
                                                      dot(second.direction, outwardDirection);
                                           });
        const double radialExtent = centerDistance + radius;
        if (!std::isfinite(radialExtent)) {
            throw std::invalid_argument("Collectible collider extent overflow");
        }

        closestArm->length = std::max(closestArm->length, radialExtent);
        m_volumeRadiusCubed = updatedVolumeRadiusCubed;
    }

    double PickupRadius() const
    {
        return std::cbrt(m_volumeRadiusCubed);
    }

    bool ContainsPickupCenter(Vector3 localCenter) const
    {
        if (!localCenter.Finite()) {
            throw std::invalid_argument("Invalid pickup center");
        }

        const double pickupRadius = PickupRadius() + PICKUP_TOLERANCE;
        return dot(localCenter, localCenter) <= pickupRadius * pickupRadius;
    }

    Vector3 PickupSurface(Vector3 localDirection) const
    {
        if (!localDirection.Finite()) {
            throw std::invalid_argument("Invalid pickup direction");
        }

        const double directionLength = localDirection.Length();
        const Vector3 direction = directionLength > MIN_DIRECTION_LENGTH
                                      ? localDirection * (1.0 / directionLength)
                                      : Vector3{0, 1, 0};
        return direction * PickupRadius();
    }

    Vector3 Support(const Quaternion& orientation, Vector3 worldDirection) const
    {
        const double directionLength = worldDirection.Length();
        if (!orientation.Unit() || !worldDirection.Finite() || !std::isfinite(directionLength) ||
            directionLength == 0) {
            throw std::invalid_argument("Invalid collider support direction");
        }

        worldDirection = worldDirection * (1.0 / directionLength);
        Vector3 bestPoint = worldDirection * m_coreRadius;
        double bestProjection = m_coreRadius;

        for (const auto& arm : m_arms) {
            const Vector3 candidate = orientation.Rotate(arm.direction * arm.length);
            const double projection = dot(candidate, worldDirection);
            if (projection > bestProjection) {
                bestPoint = candidate;
                bestProjection = projection;
            }
        }
        return bestPoint;
    }

  private:
    static constexpr double MIN_DIRECTION_LENGTH = 1e-12;
    static constexpr double PICKUP_TOLERANCE = 1e-12;
    static constexpr std::size_t AXIS_ARM_COUNT = 6;
    static constexpr std::size_t FIBONACCI_ARM_COUNT = RADIAL_ARM_COUNT - AXIS_ARM_COUNT;
    static constexpr double GOLDEN_ANGLE = 2.39996322972865332;

    void InitializeArms()
    {
        constexpr std::array<Vector3, AXIS_ARM_COUNT> AXIS_DIRECTIONS{{
            {1, 0, 0},
            {-1, 0, 0},
            {0, 1, 0},
            {0, -1, 0},
            {0, 0, 1},
            {0, 0, -1},
        }};

        std::size_t armIndex = 0;
        for (const auto direction : AXIS_DIRECTIONS) {
            m_arms[armIndex++] = {direction, m_coreRadius};
        }

        for (std::size_t index = 0; index < FIBONACCI_ARM_COUNT; ++index) {
            const double fraction =
                (static_cast<double>(index) + 0.5) / static_cast<double>(FIBONACCI_ARM_COUNT);
            const double y = 1 - 2 * fraction;
            const double horizontalRadius = std::sqrt(std::max(0.0, 1 - y * y));
            const double angle = GOLDEN_ANGLE * static_cast<double>(index);
            const Vector3 direction{std::cos(angle) * horizontalRadius, y,
                                    std::sin(angle) * horizontalRadius};
            m_arms[armIndex++] = {direction, m_coreRadius};
        }
    }

    double m_coreRadius{};
    double m_volumeRadiusCubed{};
    std::array<RadialColliderArm, RADIAL_ARM_COUNT> m_arms{};
};

} // namespace MoongTangE
