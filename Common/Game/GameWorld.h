#pragma once

#include "Common/Game/PlayerController.h"
#include "Common/Physics/Collision/SphereSphere.h"
#include "Common/Physics/CompoundBallCollider.h"
#include "Common/Physics/CompoundMassProperties.h"
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
#include <set>
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
        std::map<std::uint32_t, BodyTransform> previousPlayers;
        for (auto& [id, player] : m_players) {
            if (player.deathTick != 0) {
                if (m_tick - player.inputTick < simulation::INPUT_TIMEOUT_TICKS &&
                    player.input.restartDeathTick == player.deathTick) {
                    TryRestart(id, player);
                }
                if (player.deathTick != 0) {
                    continue;
                }
            }
            previousPlayers.emplace(id,
                                    BodyTransform{player.body.position, player.body.orientation});
            UpdateMassProperties(id, player);

            const bool inputFresh = m_tick - player.inputTick < simulation::INPUT_TIMEOUT_TICKS;
            const double massRatio = player.body.mass / simulation::PLAYER_MASS;
            const double forceScale =
                std::min(simulation::ATTACHED_MOVEMENT_FORCE_LIMIT,
                         std::pow(massRatio, simulation::ATTACHED_MOVEMENT_MASS_EXPONENT));
            const auto force = inputFresh
                                   ? movement_force(player.input.x, player.input.z) * forceScale
                                   : Vector3{};
            m_physics.Step(player.body, PlayerCollider(id, player), PhysicsWorld::FIXED_DELTA,
                           force);
        }

        ResolvePlayerCollisions(previousPlayers);

        for (auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId != 0) {
                AdvanceAttachment(collectible);
                UpdateAttached(collectible, m_players.at(collectible.attachedPlayerId));
                continue;
            }

            const BodyTransform previousCollectible{collectible.body.position,
                                                    collectible.body.orientation};
            m_physics.Step(collectible.body);
            TryAttach(collectible, previousCollectible, previousPlayers);
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
            result.players.push_back({id,
                                      body.position,
                                      body.velocity,
                                      body.orientation,
                                      body.angularVelocity,
                                      {},
                                      player.deathTick});
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

    struct BodyTransform
    {
        Vector3 position;
        Quaternion orientation;
    };

    struct Player
    {
        RigidBody body;
        std::size_t spawnSlot{};

        InputCommand input;
        std::uint64_t inputTick{};
        std::uint64_t deathTick{};
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
        Vector3 initialLocalPosition{};
        Vector3 targetLocalPosition{};
        Quaternion initialLocalOrientation{};
        Quaternion targetLocalOrientation{};
        std::uint32_t attachmentTicks{};
    };

    CompoundBallCollider PlayerCollider(std::uint32_t playerId, const Player& player) const
    {
        CompoundBallCollider collider(player.body.radius);
        for (const auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId == playerId) {
                collider.AddCollectible(collectible.localPosition, collectible.localOrientation,
                                        collectible.kind, collectible.body.radius);
            }
        }
        return collider;
    }

    void UpdateMassProperties(std::uint32_t playerId, Player& player) const
    {
        CompoundMassProperties properties;
        properties.AddSphere(simulation::PLAYER_MASS, simulation::PLAYER_RADIUS);
        for (const auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId == playerId) {
                properties.AddSphere(collectible.body.mass, collectible.body.radius,
                                     collectible.localPosition);
            }
        }
        player.body.mass = properties.Mass();
        player.body.rotationalInertia = properties.Inertia();
    }

    double PlayerVolume(std::uint32_t playerId) const
    {
        // The common 4*pi/3 factor cancels. Count each object once, not its proxy spheres.
        double volume =
            simulation::PLAYER_RADIUS * simulation::PLAYER_RADIUS * simulation::PLAYER_RADIUS;
        for (const auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId == playerId) {
                const double radius = collectible.body.radius;
                volume += radius * radius * radius;
            }
        }
        return volume;
    }

    static bool PlayersTouch(const Player& first, const BodyTransform& previousFirst,
                             const CompoundBallCollider& firstCollider, const Player& second,
                             const BodyTransform& previousSecond,
                             const CompoundBallCollider& secondCollider)
    {
        // Sweep every first-player proxy in the second player's local frame, as for pickups.
        const auto inverseStart = previousSecond.orientation.Conjugate();
        const auto inverseEnd = second.body.orientation.Conjugate();
        for (const auto& sphere : firstCollider.Spheres()) {
            const auto start = inverseStart.Rotate(previousFirst.position +
                                                   previousFirst.orientation.Rotate(sphere.center) -
                                                   previousSecond.position);
            const auto end = inverseEnd.Rotate(first.body.position +
                                               first.body.orientation.Rotate(sphere.center) -
                                               second.body.position);
            SweptSphereContact contact;
            if (secondCollider.SweepSphere(start, end, sphere.radius, contact)) {
                return true;
            }
        }
        return false;
    }

    void DefeatPlayer(std::uint32_t id, Player& player)
    {
        DetachCollectibles(id, player);
        player.deathTick = m_tick + 1;
        player.body.velocity = {};
        player.body.angularVelocity = {};
        player.input.x = 0;
        player.input.z = 0;
        player.input.restartDeathTick = 0;
    }

    bool CanRestartAt(Vector3 position) const
    {
        constexpr double SAFE_GAP = 2;
        for (const auto& [id, player] : m_players) {
            if (player.deathTick != 0) {
                continue;
            }
            const auto collider = PlayerCollider(id, player);
            for (const auto& sphere : collider.Spheres()) {
                const auto center =
                    player.body.position + player.body.orientation.Rotate(sphere.center);
                if (std::hypot(position.x - center.x, position.z - center.z) <
                    simulation::PLAYER_RADIUS + sphere.radius + SAFE_GAP) {
                    return false;
                }
            }
        }
        return std::all_of(
            m_collectibles.begin(), m_collectibles.end(), [&](const auto& collectible) {
                return std::hypot(position.x - collectible.body.position.x,
                                  position.z - collectible.body.position.z) >=
                       simulation::PLAYER_RADIUS + collectible.body.radius + SAFE_GAP;
            });
    }

    bool TryRestart(std::uint32_t id, Player& player)
    {
        constexpr std::size_t COLUMNS = 9;
        constexpr double EXTENT = simulation::COLLECTIBLE_AREA_HALF_EXTENT - 2;
        constexpr double SPACING = 2 * EXTENT / (COLUMNS - 1);
        for (std::size_t attempt = 0; attempt < COLUMNS * COLUMNS; ++attempt) {
            const auto slot = (player.spawnSlot + attempt) % (COLUMNS * COLUMNS);
            const auto row = slot / COLUMNS;
            Vector3 position{-EXTENT + static_cast<double>(slot % COLUMNS) * SPACING, 0,
                             -EXTENT + static_cast<double>(row) * SPACING};
            if (!CanRestartAt(position)) {
                continue;
            }
            position.y = terrain_sample(position.x, position.z).height + simulation::SPAWN_HEIGHT;
            player.body = {};
            player.body.position = position;
            player.deathTick = 0;
            player.input = {id, player.input.sequence, 0, 0};
            player.inputTick = m_tick;
            return true;
        }
        return false;
    }

    void ResolvePlayerCollisions(const std::map<std::uint32_t, BodyTransform>& previousPlayers)
    {
        std::map<std::uint32_t, CompoundBallCollider> colliders;
        std::map<std::uint32_t, double> volumes;
        for (const auto& [id, player] : m_players) {
            if (player.deathTick == 0) {
                colliders.emplace(id, PlayerCollider(id, player));
                volumes.emplace(id, PlayerVolume(id));
            }
        }

        // Decide simultaneous defeats before mutating ownership or resolving penetration.
        std::set<std::uint32_t> defeated;
        for (auto first = colliders.begin(); first != colliders.end(); ++first) {
            for (auto second = std::next(first); second != colliders.end(); ++second) {
                const double firstVolume = volumes.at(first->first);
                const double secondVolume = volumes.at(second->first);
                constexpr double RELATIVE_SIZE_TOLERANCE = 1e-9;
                if (std::abs(firstVolume - secondVolume) <=
                    RELATIVE_SIZE_TOLERANCE * std::max(firstVolume, secondVolume)) {
                    continue;
                }
                if (PlayersTouch(m_players.at(first->first), previousPlayers.at(first->first),
                                 first->second, m_players.at(second->first),
                                 previousPlayers.at(second->first), second->second)) {
                    defeated.insert(firstVolume < secondVolume ? first->first : second->first);
                }
            }
        }
        for (const auto id : defeated) {
            DefeatPlayer(id, m_players.at(id));
            colliders.erase(id);
        }

        for (int iteration = 0; iteration < COLLISION_SOLVER_ITERATIONS; ++iteration) {
            for (auto first = colliders.begin(); first != colliders.end(); ++first) {
                for (auto second = std::next(first); second != colliders.end(); ++second) {
                    auto& firstBody = m_players.at(first->first).body;
                    auto& secondBody = m_players.at(second->first).body;
                    CompoundSphereContact contact;
                    if (first->second.Contact(firstBody.position, firstBody.orientation,
                                              second->second, secondBody.position,
                                              secondBody.orientation, contact)) {
                        resolve_body_contact(firstBody, secondBody, contact.normal,
                                             contact.penetration);
                    }
                }
            }
            for (const auto& [id, collider] : colliders) {
                m_physics.ConstrainToTerrain(m_players.at(id).body, collider);
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

    static void AdvanceAttachment(Collectible& collectible)
    {
        if (collectible.attachmentTicks >= simulation::ATTACHMENT_SETTLE_TICKS) {
            return;
        }

        ++collectible.attachmentTicks;
        const double progress = static_cast<double>(collectible.attachmentTicks) /
                                static_cast<double>(simulation::ATTACHMENT_SETTLE_TICKS);
        constexpr double DAMPING = 5;
        const double fraction = (1 - std::exp(-DAMPING * progress)) / (1 - std::exp(-DAMPING));
        collectible.localPosition = collectible.initialLocalPosition * (1 - fraction) +
                                    collectible.targetLocalPosition * fraction;
        collectible.localOrientation = interpolate(collectible.initialLocalOrientation,
                                                   collectible.targetLocalOrientation, fraction);
    }

    static Quaternion AlignUp(Quaternion orientation, Vector3 targetUp)
    {
        constexpr double MIN_AXIS_LENGTH = 1e-12;
        const Vector3 currentUp = orientation.Rotate({0, 1, 0});
        Vector3 axis = cross(currentUp, targetUp);
        const double sine = axis.Length();
        const double cosine = std::clamp(dot(currentUp, targetUp), -1.0, 1.0);
        if (sine < MIN_AXIS_LENGTH) {
            if (cosine > 0) {
                return orientation;
            }
            axis =
                cross(currentUp, std::abs(currentUp.x) < 0.9 ? Vector3{1, 0, 0} : Vector3{0, 0, 1})
                    .Normalized();
        } else {
            axis = axis * (1.0 / sine);
        }

        const auto alignment = Quaternion::FromRotation(axis * std::atan2(sine, cosine));
        return (alignment * orientation).Normalized();
    }

    void TryAttach(Collectible& collectible, const BodyTransform& previousCollectible,
                   const std::map<std::uint32_t, BodyTransform>& previousPlayers)
    {
        struct AttachmentHit
        {
            std::uint32_t playerId{};
            double fraction{1};
            std::size_t colliderSphere{};
            std::size_t collectibleSphere{};
            Vector3 normal{};
        };

        AttachmentHit bestHit;
        bool found = false;
        const auto& shape = collectible_proxy_shape(collectible.kind);
        for (const auto& [playerId, player] : m_players) {
            if (player.deathTick != 0) {
                continue;
            }
            const auto& previousPlayer = previousPlayers.at(playerId);
            const Quaternion inversePreviousPlayer = previousPlayer.orientation.Conjugate();
            const Quaternion inversePlayer = player.body.orientation.Conjugate();
            const auto collider = PlayerCollider(playerId, player);

            for (std::size_t sphereIndex = 0; sphereIndex < shape.count; ++sphereIndex) {
                const auto& sphere = shape.spheres[sphereIndex];
                const Vector3 previousOffset =
                    previousCollectible.orientation.Rotate(sphere.center * collectible.body.radius);
                const Vector3 currentOffset =
                    collectible.body.orientation.Rotate(sphere.center * collectible.body.radius);
                const Vector3 localStart = inversePreviousPlayer.Rotate(
                    previousCollectible.position + previousOffset - previousPlayer.position);
                const Vector3 localEnd = inversePlayer.Rotate(collectible.body.position +
                                                              currentOffset - player.body.position);

                SweptSphereContact contact;
                if (!collider.SweepSphere(localStart, localEnd,
                                          sphere.radius * collectible.body.radius, contact) ||
                    (found && contact.fraction >= bestHit.fraction)) {
                    continue;
                }
                bestHit = {playerId, contact.fraction, contact.colliderSphere, sphereIndex,
                           contact.normal};
                found = true;
            }
        }
        if (!found) {
            return;
        }

        const auto& player = m_players.at(bestHit.playerId);
        const auto& previousPlayer = previousPlayers.at(bestHit.playerId);
        const Quaternion inversePreviousPlayer = previousPlayer.orientation.Conjugate();
        const Quaternion inversePlayer = player.body.orientation.Conjugate();
        const Vector3 localStart =
            inversePreviousPlayer.Rotate(previousCollectible.position - previousPlayer.position);
        const Vector3 localEnd =
            inversePlayer.Rotate(collectible.body.position - player.body.position);
        const Quaternion previousLocalOrientation =
            (inversePreviousPlayer * previousCollectible.orientation).Normalized();
        const Quaternion currentLocalOrientation =
            (inversePlayer * collectible.body.orientation).Normalized();

        const auto collider = PlayerCollider(bestHit.playerId, player);
        const auto& surfaceSphere = collider.Spheres()[bestHit.colliderSphere];
        const auto& proxySphere = shape.spheres[bestHit.collectibleSphere];
        collectible.attachedPlayerId = bestHit.playerId;
        collectible.initialLocalPosition =
            localStart * (1 - bestHit.fraction) + localEnd * bestHit.fraction;
        collectible.initialLocalOrientation =
            interpolate(previousLocalOrientation, currentLocalOrientation, bestHit.fraction);
        collectible.targetLocalOrientation =
            AlignUp(collectible.initialLocalOrientation, bestHit.normal);
        const LocalSphereProxy objectSphere{proxySphere.center * collectible.body.radius,
                                            proxySphere.radius * collectible.body.radius};
        collectible.targetLocalPosition = CompoundBallCollider::EmbeddedOrigin(
            surfaceSphere, bestHit.normal, collectible.targetLocalOrientation, objectSphere,
            collectible.body.radius * simulation::COLLECTIBLE_EMBED_RATIO);
        collectible.localPosition = collectible.initialLocalPosition;
        collectible.localOrientation = collectible.initialLocalOrientation;
        collectible.attachmentTicks = 0;
        UpdateAttached(collectible, player);
    }

    void ResetAttachment(Collectible& collectible)
    {
        collectible.attachedPlayerId = 0;
        collectible.localPosition = {};
        collectible.localOrientation = {};
        collectible.initialLocalPosition = {};
        collectible.targetLocalPosition = {};
        collectible.initialLocalOrientation = {};
        collectible.targetLocalOrientation = {};
        collectible.attachmentTicks = 0;
    }

    void DetachCollectibles(std::uint32_t playerId, const Player& player)
    {
        for (auto& collectible : m_collectibles) {
            if (collectible.attachedPlayerId != playerId) {
                continue;
            }

            UpdateAttached(collectible, player);
            ResetAttachment(collectible);
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
