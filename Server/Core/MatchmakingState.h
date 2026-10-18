#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace MoongTangE {

enum class ReservationStatus
{
    UNKNOWN,
    PENDING,
    RESERVED,
    CONSUMED,
    FULL,
    EXPIRED,
    CANCELLED,
    UNAVAILABLE
};

struct ReservationResult
{
    ReservationStatus status{ReservationStatus::UNKNOWN};
    std::uint64_t roomId{};
    std::int64_t expiresInMs{};
};

struct RoomOccupancy
{
    std::uint64_t roomId{};
    std::size_t players{};
    std::size_t reserved{};
};

struct MatchmakingState
{
    bool running{};
    std::uint64_t tick{};
    std::chrono::steady_clock::time_point updatedAt{};
    std::vector<RoomOccupancy> rooms;
};

} // namespace MoongTangE
