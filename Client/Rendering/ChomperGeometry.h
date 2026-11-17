#pragma once

#include "Common/Math/Vector3.h"

#include <cmath>
#include <numbers>

namespace MoongTangE {

inline double mouth_angle(double time)
{
    // Keep a small opening at the closed pose so both jaw surfaces remain well defined.
    return 0.06 + 0.64 * (0.5 + 0.5 * std::sin(time * 2 * std::numbers::pi * 2.4));
}

inline Vector3 chomper_surface(double latitude, double longitude, double opening)
{
    const double phi = latitude * std::numbers::pi;
    const double theta = opening + longitude * (2 * std::numbers::pi - 2 * opening);
    return {std::cos(phi), std::sin(phi) * std::sin(theta), -std::sin(phi) * std::cos(theta)};
}

} // namespace MoongTangE
