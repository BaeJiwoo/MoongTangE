#pragma once
#include "Server/ServerOptions.h"
#include <atomic>
#include <cstdint>

namespace sandbox {
// Single-use application. run() owns the server lifetime on its calling thread.
// requestStop() and port() may be called concurrently; destruction requires run() to return.
class ServerApplication {
public:
    explicit ServerApplication(ServerOptions options);
    ServerApplication(const ServerApplication&) = delete;
    ServerApplication& operator=(const ServerApplication&) = delete;
    ServerApplication(ServerApplication&&) = delete;
    ServerApplication& operator=(ServerApplication&&) = delete;
    ~ServerApplication() = default;

    void run();
    void requestStop() noexcept;
    // Zero until listening; retains the bound port after shutdown for diagnostics.
    std::uint16_t port() const noexcept;

private:
    ServerOptions options_;
    std::atomic<bool> started_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<std::uint16_t> port_{};
};
} // namespace sandbox
