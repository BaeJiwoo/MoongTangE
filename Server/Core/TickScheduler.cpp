#include "Server/Core/TickScheduler.h"

namespace MoongTangE {

TickScheduler::TickScheduler(Clock::time_point now) : m_nextTick(now) {}

TickScheduler::Clock::time_point TickScheduler::Advance(Clock::time_point now)
{
    m_nextTick += TICK_PERIOD;
    if (now > m_nextTick + TICK_PERIOD * MAX_CATCH_UP_TICKS) {
        m_nextTick = now + TICK_PERIOD;
    }
    return m_nextTick;
}

} // namespace MoongTangE
