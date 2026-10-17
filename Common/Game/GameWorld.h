#pragma once
#include "Common/Game/PlayerController.h"
#include "Common/Physics/PhysicsWorld.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/SimulationConfig.h"
#include <cstdint>
#include <map>

namespace sandbox {
// Only the server tick thread owns and mutates a GameWorld.
class GameWorld {
public:
    bool addPlayer(std::uint32_t id) {
        if (id == 0 || players_.size() >= maxPlayers || players_.contains(id)) {
            return false;
        }
        Player player;
        player.body.position.x = simulation::spawnX(id);
        players_.emplace(id, player);
        return true;
    }

    void removePlayer(std::uint32_t id) {
        players_.erase(id);
    }

    bool apply(InputCommand input) {
        const auto player = players_.find(input.playerId);
        if (!input.valid() || player == players_.end() ||
            input.sequence <= player->second.input.sequence) {
            return false;
        }
        player->second.input = input;
        player->second.inputTick = tick_;
        return true;
    }

    void step() {
        for (auto& [id, player] : players_) {
            (void)id;
            const bool inputFresh = tick_ - player.inputTick < simulation::inputTimeoutTicks;
            const auto force =
                inputFresh ? movementForce(player.input.x, player.input.z) : Vector3{};
            physics_.step(player.body, PhysicsWorld::fixedDelta, force);
        }
        ++tick_;
    }

    WorldSnapshot snapshot() const {
        WorldSnapshot result{tick_, {}};
        result.players.reserve(players_.size());
        for (const auto& [id, player] : players_) {
            const auto& body = player.body;
            result.players.push_back(
                {id, body.position, body.velocity, body.orientation, body.angularVelocity});
        }
        return result;
    }

private:
    struct Player {
        RigidBody body;
        InputCommand input;
        std::uint64_t inputTick{};
    };

    std::map<std::uint32_t, Player> players_;
    PhysicsWorld physics_;
    std::uint64_t tick_{};
};
} // namespace sandbox