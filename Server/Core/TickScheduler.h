#pragma once
#include "Common/SimulationConfig.h"
#include <chrono>

namespace sandbox {
class TickScheduler {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto period =
        std::chrono::nanoseconds(1'000'000'000 / simulation::ticksPerSecond);

    explicit TickScheduler(Clock::time_point now = Clock::now()) : next_(now) {}

    Clock::time_point advance(Clock::time_point now = Clock::now()) {
        next_ += period;
        if (now > next_ + period * maximumCatchUpTicks) {
            next_ = now + period;
        }
        return next_;
    }

private:
    static constexpr unsigned maximumCatchUpTicks = 8;
    Clock::time_point next_;
};
} // namespace sandbox
