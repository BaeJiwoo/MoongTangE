#pragma once
#include <chrono>
namespace sandbox {
class TickScheduler {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto period = std::chrono::nanoseconds(1'000'000'000/60);
    explicit TickScheduler(Clock::time_point now=Clock::now()) : next_(now) {}
    Clock::time_point advance(Clock::time_point now=Clock::now()) {
        next_ += period;
        if (now > next_+period*8) next_ = now+period;
        return next_;
    }
private:
    Clock::time_point next_;
};
}
