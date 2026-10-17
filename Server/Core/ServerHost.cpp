#include "Server/Core/ServerHost.h"
#include "Server/Core/TickScheduler.h"
#include <limits>
namespace sandbox {
void ServerHost::start() {
    std::lock_guard lifecycle(lifecycleMutex_);
    if (running_.exchange(true)) return;
    try { thread_ = std::jthread([this](std::stop_token stop) { run(stop); }); }
    catch (...) { running_ = false; throw; }
}
void ServerHost::stop() {
    std::lock_guard lifecycle(lifecycleMutex_);
    running_ = false;
    if (thread_.joinable()) { thread_.request_stop(); thread_.join(); }
    std::lock_guard lock(dataMutex_);
    commands_.clear(); connections_.clear(); snapshot_ = {};
}
std::uint32_t ServerHost::connect() {
    std::lock_guard lock(dataMutex_);
    if (!running_ || connections_.size() >= maxPlayers || commands_.size() >= maxPending || nextId_ == 0) return 0;
    const auto id = nextId_++;
    connections_.emplace(id,0);
    commands_.push_back({Kind::Join,id});
    return id;
}
void ServerHost::disconnect(std::uint32_t id) {
    std::lock_guard lock(dataMutex_);
    if (connections_.erase(id)) commands_.push_back({Kind::Leave,id});
    // Leaves may exceed maxPending by at most maxPlayers, so cleanup is never dropped.
}
bool ServerHost::submit(InputCommand input) {
    if (!input.valid()) return false;
    std::lock_guard lock(dataMutex_);
    const auto it = connections_.find(input.playerId);
    if (!running_ || it == connections_.end() || input.sequence <= it->second || commands_.size() >= maxPending) return false;
    it->second = input.sequence;
    commands_.push_back({Kind::Input,input.playerId,input});
    return true;
}
WorldSnapshot ServerHost::snapshot() const { std::lock_guard lock(dataMutex_); return snapshot_; }
std::size_t ServerHost::pending() const { std::lock_guard lock(dataMutex_); return commands_.size(); }
bool ServerHost::hasClient(std::uint32_t id) const { std::lock_guard lock(dataMutex_); return running_ && connections_.contains(id); }
void ServerHost::run(std::stop_token stop) {
    GameWorld world;
    TickScheduler scheduler;
    while (!stop.stop_requested()) {
        std::deque<Command> commands;
        { std::lock_guard lock(dataMutex_); commands.swap(commands_); }
        for (const auto& command : commands) {
            switch (command.kind) {
            case Kind::Join: world.addPlayer(command.id); break;
            case Kind::Input: world.apply(command.input); break;
            case Kind::Leave: world.removePlayer(command.id); break;
            }
        }
        world.step();
        auto state = world.snapshot();
        if (state.tick%3 == 0) { std::lock_guard lock(dataMutex_); snapshot_ = std::move(state); }
        std::this_thread::sleep_until(scheduler.advance());
    }
}
}
