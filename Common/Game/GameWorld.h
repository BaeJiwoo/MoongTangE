#pragma once
#include "Common/Game/PlayerController.h"
#include "Common/Physics/PhysicsWorld.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <map>
namespace sandbox {
class GameWorld {
    struct Player { RigidBody body; InputCommand input; std::uint64_t inputTick{}; };
    std::map<std::uint32_t, Player> players_;
    PhysicsWorld physics_;
    std::uint64_t tick_{};
public:
    bool addPlayer(std::uint32_t id) {
        if (!id || players_.size() >= maxPlayers || players_.contains(id)) return false;
        Player p;
        p.body.position.x = static_cast<double>((id-1)%8)*1.5;
        players_.emplace(id, p);
        return true;
    }
    void removePlayer(std::uint32_t id) { players_.erase(id); }
    bool apply(InputCommand input) {
        const auto it = players_.find(input.playerId);
        if (!input.valid() || it == players_.end() || input.sequence <= it->second.input.sequence) return false;
        it->second.input = input;
        it->second.inputTick = tick_;
        return true;
    }
    void step() {
        for (auto& [id,p] : players_) {
            (void)id;
            const auto velocity = tick_-p.inputTick < 120 ? movementVelocity(p.input.x,p.input.z) : Vector3{};
            p.body.velocity.x = velocity.x;
            p.body.velocity.z = velocity.z;
            physics_.step(p.body);
        }
        ++tick_;
    }
    WorldSnapshot snapshot() const {
        WorldSnapshot result{tick_, {}};
        for (const auto& [id,p] : players_) result.players.push_back({id,p.body.position,p.body.velocity});
        return result;
    }
};
}
