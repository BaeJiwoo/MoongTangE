#pragma once

#include "Common/Game/PlayerController.h"
#include "Common/Physics/Collision/SphereSphere.h"
#include "Common/Physics/DeformableBallCollider.h"
#include "Common/Physics/PhysicsWorld.h"
#include "Common/Physics/Terrain.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include "Common/SimulationConfig.h"
#include "Common/Utility/DeterministicRandom.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <numbers>
#include <vector>

namespace MoongTangE {

// Only the server tick thread owns and mutates a GameWorld.
class GameWorld
{
  public:
    explicit GameWorld(std::uint64_t seed = simulation::DEFAULT_WORLD_SEED)
        : m_random(seed), m_scaleOffset(m_random.Index(simulation::MAX_COLLECTIBLES))
    {
        for (std::size_t index = 0; index < simulation::INITIAL_COLLECTIBLES; ++index) {
            SpawnCollectible(false);
        }
    }

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
            DetachCollectibles(id, player->second);
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

    std::size_t CollectibleCount() const
    {
        return m_collectibles.size();
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
            const bool inputFresh = m_tick - player.inputTick < simulation::INPUT_TIMEOUT_TICKS;
            const auto force =
                inputFresh ? movement_force(player.input.x, player.input.z) : Vector3{};
            m_physics.Step(player.body, PlayerCollider(id, player), PhysicsWorld::FIXED_DELTA,
                           force);
        }

        ResolvePlayerCollisions();

        for (auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId != 0) {
                UpdateAttached(collectible, m_players.at(collectible.attachedPlayerId));
                continue;
            }

            m_physics.Step(collectible.body);
            TryAttach(collectible);
        }

        ++m_tick;
        if (m_tick % simulation::COLLECTIBLE_SPAWN_INTERVAL_TICKS == 0) {
            SpawnCollectible(true);
        }
    }

    WorldSnapshot Snapshot() const
    {
        WorldSnapshot result{m_tick, {}, {}};
        result.players.reserve(m_players.size());
        result.collectibles.reserve(m_collectibles.size());

        for (const auto& [id, player] : m_players) {
            const auto& body = player.body;
            result.players.push_back(
                {id, body.position, body.velocity, body.orientation, body.angularVelocity, {}});
        }

        for (const auto& collectible : m_collectibles) {
            const auto& body = collectible.body;
            result.collectibles.push_back({collectible.id, collectible.kind, body.position,
                                           body.orientation, collectible.scale,
                                           collectible.attachedPlayerId});
        }
        return result;
    }

  private:
    static constexpr int COLLISION_SOLVER_ITERATIONS = 4;

    struct Player
    {
        RigidBody body;
        std::size_t spawnSlot{};

        InputCommand input;
        std::uint64_t inputTick{};
    };

    struct Collectible
    {
        std::uint32_t id{};
        CollectibleKind kind{CollectibleKind::APPLE};
        RigidBody body;
        double scale{1};
        std::uint32_t attachedPlayerId{};
        Vector3 localPosition{};
        Quaternion localOrientation{};
    };

    DeformableBallCollider PlayerCollider(std::uint32_t playerId, const Player& player) const
    {
        DeformableBallCollider collider(player.body.radius);
        for (const auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId == playerId) {
                collider.AddCollectible(collectible.localPosition, collectible.body.radius);
            }
        }
        return collider;
    }

    void ResolvePlayerCollisions()
    {
        std::map<std::uint32_t, DeformableBallCollider> colliders;
        for (const auto& [id, player] : m_players) {
            colliders.emplace(id, PlayerCollider(id, player));
        }

        for (int iteration = 0; iteration < COLLISION_SOLVER_ITERATIONS; ++iteration) {
            for (auto first = m_players.begin(); first != m_players.end(); ++first) {
                for (auto second = std::next(first); second != m_players.end(); ++second) {
                    const Vector3 separation =
                        second->second.body.position - first->second.body.position;
                    const double distance = separation.Length();
                    const Vector3 normal =
                        distance > 0 ? separation * (1.0 / distance) : Vector3{1, 0, 0};
                    const auto& firstCollider = colliders.at(first->first);
                    const auto& secondCollider = colliders.at(second->first);
                    const double minimumDistance =
                        dot(firstCollider.Support(first->second.body.orientation, normal), normal) +
                        dot(secondCollider.Support(second->second.body.orientation, normal * -1),
                            normal * -1);
                    resolve_sphere_collision(first->second.body, second->second.body,
                                             minimumDistance);
                }
            }
            for (auto& [id, player] : m_players) {
                m_physics.ConstrainToTerrain(player.body, colliders.at(id));
            }
        }
    }

    static void UpdateAttached(Collectible& collectible, const Player& player)
    {
        const Vector3 worldOffset = player.body.orientation.Rotate(collectible.localPosition);
        collectible.body.position = player.body.position + worldOffset;
        collectible.body.orientation =
            (player.body.orientation * collectible.localOrientation).Normalized();
        collectible.body.velocity =
            player.body.velocity + cross(player.body.angularVelocity, worldOffset);
        collectible.body.angularVelocity = player.body.angularVelocity;
    }

    void TryAttach(Collectible& collectible)
    {
        for (const auto& [playerId, player] : m_players) {
            const Quaternion inversePlayerOrientation = player.body.orientation.Conjugate();
            const Vector3 localObjectPosition =
                inversePlayerOrientation.Rotate(collectible.body.position - player.body.position);
            const auto collider = PlayerCollider(playerId, player);
            if (!collider.ContainsPickupCenter(localObjectPosition)) {
                continue;
            }

            collectible.attachedPlayerId = playerId;
            collectible.localPosition = collider.PickupSurface(localObjectPosition);
            collectible.localOrientation =
                (inversePlayerOrientation * collectible.body.orientation).Normalized();
            UpdateAttached(collectible, player);
            return;
        }
    }

    void DetachCollectibles(std::uint32_t playerId, const Player& player)
    {
        for (auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId != playerId) {
                continue;
            }

            UpdateAttached(collectible, player);
            collectible.attachedPlayerId = 0;
            collectible.localPosition = {};
            collectible.localOrientation = {};
        }
    }

    double CollectibleScale(std::uint32_t id) const
    {
        constexpr std::size_t SCALE_STRIDE = 5;
        const auto scaleIndex =
            (m_scaleOffset + (static_cast<std::size_t>(id) - 1) * SCALE_STRIDE) %
            simulation::MAX_COLLECTIBLES;
        const double fraction =
            static_cast<double>(scaleIndex) / static_cast<double>(simulation::MAX_COLLECTIBLES - 1);
        return simulation::COLLECTIBLE_MIN_SCALE +
               (simulation::COLLECTIBLE_MAX_SCALE - simulation::COLLECTIBLE_MIN_SCALE) * fraction;
    }

    bool CanPlaceCollectible(Vector3 position, double radius) const
    {
        return std::all_of(m_collectibles.begin(), m_collectibles.end(),
                           [&](const Collectible& existing) {
                               const Vector3 offset{position.x - existing.body.position.x, 0,
                                                    position.z - existing.body.position.z};
                               return offset.Length() >= radius + existing.body.radius +
                                                             simulation::COLLECTIBLE_MIN_GAP;
                           });
    }

    bool SpawnCollectible(bool falling)
    {
        if (m_collectibles.size() >= simulation::MAX_COLLECTIBLES) {
            return false;
        }

        Collectible collectible;
        collectible.id = m_nextCollectibleId;
        collectible.kind = static_cast<CollectibleKind>(
            static_cast<std::uint32_t>(CollectibleKind::APPLE) + m_random.Index(4));
        collectible.scale = CollectibleScale(collectible.id);
        collectible.body.radius = simulation::COLLECTIBLE_BASE_RADIUS * collectible.scale;
        collectible.body.mass = simulation::COLLECTIBLE_BASE_MASS * collectible.scale *
                                collectible.scale * collectible.scale;
        collectible.body.orientation =
            Quaternion::FromRotation({0, m_random.Range(0, 2 * std::numbers::pi), 0});

        for (std::size_t attempt = 0; attempt < simulation::COLLECTIBLE_SPAWN_ATTEMPTS; ++attempt) {
            Vector3 position{m_random.Range(-simulation::COLLECTIBLE_AREA_HALF_EXTENT,
                                            simulation::COLLECTIBLE_AREA_HALF_EXTENT),
                             0,
                             m_random.Range(-simulation::COLLECTIBLE_AREA_HALF_EXTENT,
                                            simulation::COLLECTIBLE_AREA_HALF_EXTENT)};
            if (!CanPlaceCollectible(position, collectible.body.radius)) {
                continue;
            }

            position.y =
                falling ? simulation::COLLECTIBLE_DROP_HEIGHT
                        : terrain_sample(position.x, position.z).height + collectible.body.radius;
            collectible.body.position = position;
            m_collectibles.push_back(collectible);
            ++m_nextCollectibleId;
            return true;
        }
        return false;
    }

    std::map<std::uint32_t, Player> m_players;
    std::array<bool, MAX_PLAYERS> m_spawnSlots{};
    std::vector<Collectible> m_collectibles;
    PhysicsWorld m_physics;
    DeterministicRandom m_random;
    std::size_t m_scaleOffset{};
    std::uint32_t m_nextCollectibleId{1};
    std::uint64_t m_tick{};
};

} // namespace MoongTangE
