#pragma once
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace sandbox {
class ServerHost {
public:
    static constexpr std::size_t maxPending = 1024;

    ServerHost() = default;
    ServerHost(const ServerHost&) = delete;
    ServerHost& operator=(const ServerHost&) = delete;
    ServerHost(ServerHost&&) = delete;
    ServerHost& operator=(ServerHost&&) = delete;

    ~ServerHost() {
        stop();
    }

    void start();
    void stop();

    bool running() const {
        return running_.load();
    }

    std::uint32_t connect();
    void disconnect(std::uint32_t id);
    bool submit(InputCommand input);
    WorldSnapshot snapshot() const;
    std::size_t pending() const;
    bool hasClient(std::uint32_t id) const;

private:
    enum class Kind { Join, Input, Leave };

    struct Command {
        Kind kind{Kind::Join};
        std::uint32_t id{};
        InputCommand input{};
    };

    void run(std::stop_token stop);
    // dataMutex_ protects commands, connections, snapshot and nextId; lifecycleMutex_
    // serializes start/stop. The tick thread alone owns the GameWorld inside run().
    mutable std::mutex dataMutex_;
    std::mutex lifecycleMutex_;
    std::deque<Command> commands_;
    std::map<std::uint32_t, std::uint64_t> connections_;
    WorldSnapshot snapshot_;
    std::uint32_t nextId_{1};
    std::atomic<bool> running_{false};
    std::jthread thread_;
};
} // namespace sandbox
