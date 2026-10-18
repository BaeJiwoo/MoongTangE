#pragma once

#include <cmath>
#include <cstdint>

namespace MoongTangE {

struct InputCommand
{
    std::uint32_t playerId{};
    std::uint64_t sequence{};

    double x{}, z{};

    bool Valid() const
    {
        return playerId != 0 && sequence != 0 && std::isfinite(x) && std::isfinite(z) &&
               std::abs(x) <= 1 && std::abs(z) <= 1;
    }
};

} // namespace MoongTangE
