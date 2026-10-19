#pragma once

#include "Common/Game/PlayerController.h"
#include "Common/Physics/PhysicsWorld.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/SimulationConfig.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

namespace MoongTangE {

// Only the server tick thread owns and mutates a GameWorld.
class GameWorld
{
  public:
    bool AddPlayer(std::uint32_t id)
    {
        if (id == 0 || m_players.size() >= MAX_PLAYERS || m_players.contains(id)) {
            return false;
        }

        const auto freeSlot = std::find(m_spawnSlots.begin(), m_spawnSlots.end(), false);
        Player player;
        player.spawnSlot = static_cast<std::size_t>(freeSlot - m_spawnSlots.begin());
        player.body.position.x =
            simulation::spawn_x(static_cast<std::uint32_t>(player.spawnSlot + 1));
        m_players.emplace(id, player);
        *freeSlot = true;
        return true;
    }

    void RemovePlayer(std::uint32_t id)
    {
        const auto player = m_players.find(id);
        if (player != m_players.end()) {
            m_spawnSlots[player->second.spawnSlot] = false;
            m_players.erase(player);
        }
    }

    bool HasPlayer(std::uint32_t id) const
    {
        return m_players.contains(id);
    }

    std::size_t PlayerCount() const
    {
        return m_players.size();
    }

    bool Apply(InputCommand input)
    {
        const auto player = m_players.find(input.playerId);
        if (!input.Valid() || player == m_players.end() ||
            input.sequence <= player->second.input.sequence) {
            return false;
        }

        player->second.input = input;
        player->second.inputTick = m_tick;
        return true;
    }

    void Step()
    {
        for (auto& [id, player] : m_players) {
            (void)id;
            const bool inputFresh = m_tick - player.inputTick < simulation::INPUT_TIMEOUT_TICKS;
            const auto force =
                inputFresh ? movement_force(player.input.x, player.input.z) : Vector3{};
            m_physics.Step(player.body, PhysicsWorld::FIXED_DELTA, force);
        }

        ++m_tick;
    }

    WorldSnapshot Snapshot() const
    {
        WorldSnapshot result{m_tick, {}};
        result.players.reserve(m_players.size());

        for (const auto& [id, player] : m_players) {
            const auto& body = player.body;
            result.players.push_back(
                {id, body.position, body.velocity, body.orientation, body.angularVelocity, {}});
        }
        return result;
    }

  private:
    struct Player
    {
        RigidBody body;
        std::size_t spawnSlot{};

        InputCommand input;
        std::uint64_t inputTick{};
    };

    std::map<std::uint32_t, Player> m_players;
    std::array<bool, MAX_PLAYERS> m_spawnSlots{};
    PhysicsWorld m_physics;
    std::uint64_t m_tick{};
};

} // namespace MoongTangE
