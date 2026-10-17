#pragma once
#include "Common/Math/Vector3.h"

namespace sandbox {
struct Quaternion {
    double w{1}, x{}, y{}, z{};
    constexpr bool operator==(const Quaternion&) const = default;

    bool finite() const {
        return std::isfinite(w) && Vector3{x, y, z}.finite();
    }

    double norm() const {
        return std::hypot(std::hypot(w, x), std::hypot(y, z));
    }

    bool unit() const {
        return finite() && std::abs(norm() - 1) < 1e-6;
    }

    Quaternion normalized() const {
        const double n = norm();
        if (!finite() || !std::isfinite(n) || n < 1e-12) {
            throw std::invalid_argument("Invalid quaternion");
        }
        return {w / n, x / n, y / n, z / n};
    }

    Quaternion operator*(Quaternion q) const {
        return {w * q.w - x * q.x - y * q.y - z * q.z, w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x, w * q.z + x * q.y - y * q.x + z * q.w};
    }

    Vector3 rotate(Vector3 v) const {
        const Vector3 u{x, y, z};
        return v + 2.0 * cross(u, cross(u, v) + w * v);
    }

    static Quaternion fromRotation(Vector3 rotation) {
        const double angle = rotation.length();
        if (!rotation.finite() || !std::isfinite(angle)) {
            throw std::invalid_argument("Invalid rotation");
        }
        if (angle < 1e-12) {
            return Quaternion{1, rotation.x * 0.5, rotation.y * 0.5, rotation.z * 0.5}.normalized();
        }
        const auto axis = rotation * (std::sin(angle * 0.5) / angle);
        return {std::cos(angle * 0.5), axis.x, axis.y, axis.z};
    }
};

// q and -q represent the same rotation; choose the shortest arc.
inline Quaternion interpolate(Quaternion a, Quaternion b, double t) {
    double d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0) {
        b = {-b.w, -b.x, -b.y, -b.z};
        d = -d;
    }
    double u = 1 - t, v = t;
    if (d < 0.9995) {
        const double angle = std::acos(std::clamp(d, -1.0, 1.0));
        u = std::sin((1 - t) * angle) / std::sin(angle);
        v = std::sin(t * angle) / std::sin(angle);
    }
    return Quaternion{u * a.w + v * b.w, u * a.x + v * b.x, u * a.y + v * b.y, u * a.z + v * b.z}
        .normalized();
}
} // namespace sandbox
