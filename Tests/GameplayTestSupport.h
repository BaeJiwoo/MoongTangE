#pragma once

#include "Common/Transport/IClientTransport.h"
#include "Tests/TestSupport.h"

#include <chrono>

namespace MoongTangE::test {

inline void wait_for_gameplay(IClientTransport& client)
{
    check(wait_for(
              [&] {
                  const auto state = client.Receive();
                  return state && state->round.phase == RoundPhase::PLAYING;
              },
              std::chrono::seconds(6)),
          "Server countdown completed");
}

} // namespace MoongTangE::test
