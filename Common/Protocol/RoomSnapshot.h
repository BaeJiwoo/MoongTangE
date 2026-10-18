#pragma once

#include "Common/Protocol/WorldSnapshot.h"

#include <cstdint>

namespace MoongTangE {

struct RoomSnapshot
{
    std::uint64_t roomId{};
    WorldSnapshot world;
};

} // namespace MoongTangE
