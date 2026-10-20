#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace MoongTangE {

// SplitMix64 with explicit integer-to-double conversion; results do not depend on STL engines.
class DeterministicRandom
{
  public:
    explicit DeterministicRandom(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t Next()
    {
        m_state += 0x9e3779b97f4a7c15ULL;
        auto value = m_state;
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    std::size_t Index(std::size_t exclusiveMaximum)
    {
        if (exclusiveMaximum == 0) {
            throw std::invalid_argument("Random index requires a nonzero range");
        }

        const auto range = static_cast<std::uint64_t>(exclusiveMaximum);
        const auto rejectionThreshold = -range % range;
        std::uint64_t value = Next();
        while (value < rejectionThreshold) {
            value = Next();
        }
        return static_cast<std::size_t>(value % range);
    }

    double Range(double minimum, double maximum)
    {
        if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum) {
            throw std::invalid_argument("Invalid random range");
        }

        constexpr double UNIT_SCALE = 1.0 / 9007199254740992.0;
        const double fraction = static_cast<double>(Next() >> 11U) * UNIT_SCALE;
        return minimum + (maximum - minimum) * fraction;
    }

  private:
    std::uint64_t m_state;
};

} // namespace MoongTangE
