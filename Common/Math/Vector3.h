#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sandbox {
struct Vector3 {
    double x{}, y{}, z{};

    constexpr Vector3 operator+(Vector3 v) const {
        return {x + v.x, y + v.y, z + v.z};
    }

    constexpr Vector3 operator-(Vector3 v) const {
        return {x - v.x, y - v.y, z - v.z};
    }

    constexpr Vector3 operator*(double s) const {
        return {x * s, y * s, z * s};
    }

    constexpr bool operator==(const Vector3&) const = default;

    bool finite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    double length() const {
        return std::hypot(x, y, z);
    }

    Vector3 normalized() const {
        if (!finite()) {
            throw std::invalid_argument("Cannot normalize a non-finite vector");
        }
        const double scale = std::max({std::abs(x), std::abs(y), std::abs(z)});
        if (scale == 0) {
            return {};
        }
        const Vector3 v{x / scale, y / scale, z / scale};
        return v * (1.0 / v.length());
    }
};

inline constexpr Vector3 operator*(double s, Vector3 v) {
    return v * s;
}

inline constexpr double dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline constexpr Vector3 cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
} // namespace sandbox
