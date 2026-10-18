#pragma once

#include "Server/ServerOptions.h"

#include <atomic>
#include <cstdint>

namespace MoongTangE {

// Single-use application. run() owns the server lifetime on its calling thread.
// requestStop() and port() may be called concurrently; destruction requires run() to return.
class ServerApplication
{
  public:
    explicit ServerApplication(ServerOptions options);

    ServerApplication(const ServerApplication&) = delete;
    ServerApplication& operator=(const ServerApplication&) = delete;
    ServerApplication(ServerApplication&&) = delete;
    ServerApplication& operator=(ServerApplication&&) = delete;
    ~ServerApplication() = default;

    void Run();
    void RequestStop() noexcept;

    // Zero until listening; retains the bound port after shutdown for diagnostics.
    std::uint16_t Port() const noexcept;

  private:
    ServerOptions m_options;

    std::atomic<bool> m_hasRun{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<std::uint16_t> m_boundPort{};
};

} // namespace MoongTangE
