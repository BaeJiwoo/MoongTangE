#pragma once

#include "Common/Math/Vector3.h"
#include "Common/Protocol/WorldSnapshot.h"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace MoongTangE {

struct NormalizedSphereProxy
{
    Vector3 center{};
    double radius{};
};

struct CollectibleProxyShape
{
    static constexpr std::size_t MAX_SPHERES = 6;

    std::array<NormalizedSphereProxy, MAX_SPHERES> spheres{};
    std::size_t count{};
};

inline const CollectibleProxyShape& collectible_proxy_shape(CollectibleKind kind)
{
    static constexpr CollectibleProxyShape APPLE{{{{{}, 1.0}}}, 1};
    static constexpr CollectibleProxyShape BANANA{{{{{-0.55, 0.12, 0}, 0.40},
                                                    {{-0.28, -0.02, 0}, 0.42},
                                                    {{0, -0.10, 0}, 0.43},
                                                    {{0.28, -0.02, 0}, 0.42},
                                                    {{0.55, 0.12, 0}, 0.40}}},
                                                  5};
    static constexpr CollectibleProxyShape BURGER{
        {{{{0, -0.22, 0}, 0.78}, {{0, 0, 0}, 0.82}, {{0, 0.22, 0}, 0.78}}}, 3};
    static constexpr CollectibleProxyShape DONUT{{{{{-0.35, 0, 0}, 0.65},
                                                   {{0.35, 0, 0}, 0.65},
                                                   {{0, 0, -0.35}, 0.65},
                                                   {{0, 0, 0.35}, 0.65}}},
                                                 4};

    switch (kind) {
    case CollectibleKind::APPLE:
        return APPLE;
    case CollectibleKind::BANANA:
        return BANANA;
    case CollectibleKind::BURGER:
        return BURGER;
    case CollectibleKind::DONUT:
        return DONUT;
    }
    throw std::invalid_argument("Invalid collectible kind");
}

} // namespace MoongTangE
