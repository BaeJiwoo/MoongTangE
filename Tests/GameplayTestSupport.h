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
                  return state && state->round.phase == RoundPhase::FREE_PLAY;
              },
              std::chrono::seconds(6)),
          "Server continuous gameplay started");
}

} // namespace MoongTangE::test
