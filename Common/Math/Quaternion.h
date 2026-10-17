#pragma once
#include "Common/Math/Vector3.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MoongTangE {
// Raw quaternion value. Rotation consumers validate unit length at their boundary.
struct Quaternion
{
    double w{1};
    double x{};
    double y{};
    double z{};

    constexpr bool operator==(const Quaternion&) const = default;

    bool Finite() const
    {
        return std::isfinite(w) && Vector3{x, y, z}.Finite();
    }

    double Norm() const
    {
        return std::hypot(std::hypot(w, x), std::hypot(y, z));
    }

    bool Unit() const
    {
        constexpr double UNIT_TOLERANCE = 1e-6;
        return Finite() && std::abs(Norm() - 1) < UNIT_TOLERANCE;
    }

    Quaternion Normalized() const
    {
        constexpr double MIN_NORM = 1e-12;
        const double magnitude = Norm();
        if (!Finite() || !std::isfinite(magnitude) || magnitude < MIN_NORM) {
            throw std::invalid_argument("Invalid quaternion");
        }
        return {w / magnitude, x / magnitude, y / magnitude, z / magnitude};
    }

    Quaternion operator*(Quaternion other) const
    {
        return {w * other.w - x * other.x - y * other.y - z * other.z,
                w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y - x * other.z + y * other.w + z * other.x,
                w * other.z + x * other.y - y * other.x + z * other.w};
    }

    // Precondition: this quaternion is unit length.
    Vector3 Rotate(Vector3 vector) const
    {
        const Vector3 imaginary{x, y, z};
        return vector + 2.0 * cross(imaginary, cross(imaginary, vector) + w * vector);
    }

    static Quaternion FromRotation(Vector3 rotation)
    {
        constexpr double SMALL_ANGLE = 1e-12;
        const double angle = rotation.Length();
        if (!rotation.Finite() || !std::isfinite(angle)) {
            throw std::invalid_argument("Invalid rotation");
        }
        if (angle < SMALL_ANGLE) {
            return Quaternion{1, rotation.x * 0.5, rotation.y * 0.5, rotation.z * 0.5}.Normalized();
        }
        const auto scaledAxis = rotation * (std::sin(angle * 0.5) / angle);
        return {std::cos(angle * 0.5), scaledAxis.x, scaledAxis.y, scaledAxis.z};
    }
};

// Unit inputs, fraction in [0,1]. q and -q represent the same rotation; take the shortest arc.
inline Quaternion interpolate(Quaternion from, Quaternion to, double fraction)
{
    double alignment = from.w * to.w + from.x * to.x + from.y * to.y + from.z * to.z;
    if (alignment < 0) {
        to = {-to.w, -to.x, -to.y, -to.z};
        alignment = -alignment;
    }

    double fromWeight = 1 - fraction;
    double toWeight = fraction;
    constexpr double LINEAR_THRESHOLD = 0.9995;
    if (alignment < LINEAR_THRESHOLD) {
        const double angle = std::acos(std::clamp(alignment, -1.0, 1.0));
        fromWeight = std::sin((1 - fraction) * angle) / std::sin(angle);
        toWeight = std::sin(fraction * angle) / std::sin(angle);
    }
    return Quaternion{fromWeight * from.w + toWeight * to.w, fromWeight * from.x + toWeight * to.x,
                      fromWeight * from.y + toWeight * to.y, fromWeight * from.z + toWeight * to.z}
        .Normalized();
}
} // namespace MoongTangE