#pragma once

#include "Common/Math/Vector3.h"

#include <cmath>
#include <stdexcept>

namespace MoongTangE {

class CompoundMassProperties
{
  public:
    void AddSphere(double mass, double radius, Vector3 center = {})
    {
        if (!std::isfinite(mass) || mass <= 0 || !std::isfinite(radius) || radius <= 0 ||
            !center.Finite()) {
            throw std::invalid_argument("Invalid mass element");
        }

        const double inertia = 0.4 * mass * radius * radius + mass * dot(center, center);
        if (!std::isfinite(m_mass + mass) || !std::isfinite(m_inertia + inertia)) {
            throw std::invalid_argument("Mass properties overflow");
        }
        m_mass += mass;
        m_inertia += inertia;
    }

    double Mass() const
    {
        return m_mass;
    }

    double Inertia() const
    {
        return m_inertia;
    }

  private:
    double m_mass{};
    double m_inertia{};
};

} // namespace MoongTangE
