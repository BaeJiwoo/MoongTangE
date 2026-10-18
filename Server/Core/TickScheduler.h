#pragma once

#include "Common/SimulationConfig.h"

#include <chrono>

namespace MoongTangE {

class TickScheduler
{
  public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto TICK_PERIOD =
        std::chrono::nanoseconds(1'000'000'000 / simulation::TICKS_PER_SECOND);
    explicit TickScheduler(Clock::time_point now = Clock::now());
    Clock::time_point Advance(Clock::time_point now = Clock::now());

  private:
    static constexpr unsigned MAX_CATCH_UP_TICKS = 8;
    Clock::time_point m_nextTick;
};

} // namespace MoongTangE
