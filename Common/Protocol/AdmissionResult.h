#pragma once

#include <cstdint>

namespace MoongTangE {

enum class AdmissionStatus : std::uint8_t
{
    PENDING = 0,
    ACCEPTED = 1,
    FULL = 2,
    UNAVAILABLE = 3
};

struct AdmissionResult
{
    AdmissionStatus status{AdmissionStatus::PENDING};
    std::uint32_t playerId{};
    std::uint64_t roomId{};
};

} // namespace MoongTangE
