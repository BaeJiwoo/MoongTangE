#pragma once
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/SimulationConfig.h"
#include <algorithm>
#include <cmath>

namespace MoongTangE {
// Presentation-only, approximately one snapshot interval behind; never predicts physics.
class SnapshotInterpolator {
public:
    void advance(double dt) {
        if (std::isfinite(dt) && dt > 0) {
            elapsed_ = std::min(duration_, elapsed_ + dt);
        }
    }

    void push(const WorldSnapshot& next) {
        if (initialized_ && next.tick <= target_.tick) {
            return;
        }
        from_ = initialized_ ? sample() : next;
        if (initialized_) {
            const double tickInterval =
                static_cast<double>(next.tick - target_.tick) * simulation::FIXED_DELTA;
            duration_ = std::clamp(tickInterval, simulation::FIXED_DELTA, maximumInterval);
        } else {
            duration_ = defaultInterval;
        }
        target_ = next;
        elapsed_ = 0;
        initialized_ = true;
    }

    WorldSnapshot sample() const {
        WorldSnapshot result = target_;
        const double fraction = std::clamp(elapsed_ / duration_, 0.0, 1.0);
        for (auto& player : result.players) {
            const auto previous = std::find_if(from_.players.begin(), from_.players.end(),
                                               [&](const PlayerState& candidate) {
                                                   return candidate.id == player.id;
                                               });
            if (previous != from_.players.end()) {
                interpolatePlayer(*previous, player, fraction);
            }
        }
        return result;
    }

private:
    static constexpr double defaultInterval =
        simulation::SNAPSHOT_INTERVAL_TICKS * simulation::FIXED_DELTA;
    static constexpr double maximumInterval = 0.1;

    static void interpolatePlayer(const PlayerState& from, PlayerState& to, double fraction) {
        to.position = from.position * (1 - fraction) + to.position * fraction;
        to.velocity = from.velocity * (1 - fraction) + to.velocity * fraction;
        to.angularVelocity = from.angularVelocity * (1 - fraction) + to.angularVelocity * fraction;
        to.orientation = interpolate(from.orientation, to.orientation, fraction);
    }

    WorldSnapshot from_;
    WorldSnapshot target_;
    double elapsed_{};
    double duration_{defaultInterval};
    bool initialized_{};
};
} // namespace MoongTangE