#pragma once
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/SimulationConfig.h"

#include <algorithm>
#include <cmath>

namespace MoongTangE {
// Presentation-only, approximately one snapshot interval behind; never predicts physics.
class SnapshotInterpolator
{
  public:
    void Advance(double dt)
    {
        if (std::isfinite(dt) && dt > 0) {
            m_elapsed = std::min(m_duration, m_elapsed + dt);
        }
    }

    void Push(const WorldSnapshot& next)
    {
        if (m_initialized && next.tick <= m_target.tick) {
            return;
        }
        m_from = m_initialized ? Sample() : next;
        if (m_initialized) {
            const double tickInterval =
                static_cast<double>(next.tick - m_target.tick) * simulation::FIXED_DELTA;
            m_duration = std::clamp(tickInterval, simulation::FIXED_DELTA, MAX_INTERVAL);
        } else {
            m_duration = DEFAULT_INTERVAL;
        }
        m_target = next;
        m_elapsed = 0;
        m_initialized = true;
    }

    WorldSnapshot Sample() const
    {
        WorldSnapshot result = m_target;
        const double fraction = std::clamp(m_elapsed / m_duration, 0.0, 1.0);
        for (auto& player : result.players) {
            const auto previous = std::find_if(m_from.players.begin(), m_from.players.end(),
                                               [&](const PlayerState& candidate) {
                                                   return candidate.id == player.id;
                                               });
            if (previous != m_from.players.end()) {
                InterpolatePlayer(*previous, player, fraction);
            }
        }
        return result;
    }

  private:
    static constexpr double DEFAULT_INTERVAL =
        simulation::SNAPSHOT_INTERVAL_TICKS * simulation::FIXED_DELTA;
    static constexpr double MAX_INTERVAL = 0.1;

    static void InterpolatePlayer(const PlayerState& from, PlayerState& to, double fraction)
    {
        to.position = from.position * (1 - fraction) + to.position * fraction;
        to.velocity = from.velocity * (1 - fraction) + to.velocity * fraction;
        to.angularVelocity = from.angularVelocity * (1 - fraction) + to.angularVelocity * fraction;
        to.orientation = interpolate(from.orientation, to.orientation, fraction);
    }

    WorldSnapshot m_from;
    WorldSnapshot m_target;
    double m_elapsed{};
    double m_duration{DEFAULT_INTERVAL};
    bool m_initialized{};
};
} // namespace MoongTangE