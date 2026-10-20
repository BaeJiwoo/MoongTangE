#include "Client/Rendering/BallDeformation.h"

#include "Common/SimulationConfig.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <tuple>

namespace MoongTangE {
namespace {

constexpr double ANGULAR_SPREAD = 1.75;
constexpr double PROTRUSION_RATIO = 0.15;
constexpr double MAX_HALF_ANGLE = std::numbers::pi / 2;
constexpr double MIN_DIRECTION_LENGTH = 1e-12;

double smooth_step(double value)
{
    const double clamped = std::clamp(value, 0.0, 1.0);
    return clamped * clamped * (3 - 2 * clamped);
}

double smooth_step_derivative(double value)
{
    if (value <= 0 || value >= 1) {
        return 0;
    }
    return 6 * value * (1 - value);
}

} // namespace

BallDeformation::BallDeformation(const PlayerState& player,
                                 std::span<const CollectibleState> collectibles)
{
    const Quaternion inversePlayerOrientation = player.orientation.Conjugate();
    for (const auto& collectible : collectibles) {
        if (collectible.attachedPlayerId != player.id || !std::isfinite(collectible.scale) ||
            collectible.scale <= 0) {
            continue;
        }

        const Vector3 localPosition =
            inversePlayerOrientation.Rotate(collectible.position - player.position);
        const double distance = localPosition.Length();
        if (!localPosition.Finite() || !std::isfinite(distance) ||
            distance <= MIN_DIRECTION_LENGTH) {
            continue;
        }

        const double collectibleRadius = simulation::COLLECTIBLE_BASE_RADIUS * collectible.scale;
        const double halfAngle =
            std::min(MAX_HALF_ANGLE, std::atan2(collectibleRadius, distance) * ANGULAR_SPREAD);
        m_bulges.push_back(
            {localPosition * (1.0 / distance),
             std::max(simulation::PLAYER_RADIUS, distance + collectibleRadius * PROTRUSION_RATIO),
             std::cos(halfAngle)});
    }

    std::sort(m_bulges.begin(), m_bulges.end(), [](const Bulge& left, const Bulge& right) {
        return std::tie(left.direction.x, left.direction.y, left.direction.z, left.targetRadius,
                        left.cutoffAlignment) < std::tie(right.direction.x, right.direction.y,
                                                         right.direction.z, right.targetRadius,
                                                         right.cutoffAlignment);
    });
}

DeformedBallVertex BallDeformation::Sample(Vector3 direction) const
{
    const double directionLength = direction.Length();
    if (!direction.Finite() || !std::isfinite(directionLength) ||
        directionLength <= MIN_DIRECTION_LENGTH) {
        throw std::invalid_argument("Ball deformation direction must be finite and nonzero");
    }

    direction = direction * (1.0 / directionLength);
    double radius = simulation::PLAYER_RADIUS;
    Vector3 tangentGradient{};

    for (const auto& bulge : m_bulges) {
        const double alignment = dot(direction, bulge.direction);
        const double alignmentRange = 1 - bulge.cutoffAlignment;
        const double normalizedAlignment = (alignment - bulge.cutoffAlignment) / alignmentRange;
        const double influence = smooth_step(normalizedAlignment);
        const double radialIncrease = bulge.targetRadius - simulation::PLAYER_RADIUS;
        const double candidateRadius = simulation::PLAYER_RADIUS + radialIncrease * influence;
        if (candidateRadius <= radius) {
            continue;
        }

        radius = candidateRadius;
        const double derivative =
            radialIncrease * smooth_step_derivative(normalizedAlignment) / alignmentRange;
        const Vector3 gradient = bulge.direction * derivative;
        tangentGradient = gradient - direction * dot(gradient, direction);
    }

    return {direction * radius, (direction * radius - tangentGradient).Normalized()};
}

} // namespace MoongTangE
