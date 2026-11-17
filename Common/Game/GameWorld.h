#pragma once

#include "Common/Game/ArenaLayout.h"
#include "Common/Physics/Collision/SphereSphere.h"
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
    explicit GameWorld(std::uint64_t seed = simulation::DEFAULT_WORLD_SEED,
                       std::uint64_t initialTick = 0)
        : m_random(seed), m_tick(initialTick)
    {
        constexpr std::array<Vector3, simulation::MAX_MINES> POSITIONS{
            {{-8, simulation::MINE_RADIUS, -8},
             {8, simulation::MINE_RADIUS, -8},
             {-8, simulation::MINE_RADIUS, 8},
             {8, simulation::MINE_RADIUS, 8},
             {-17, simulation::MINE_RADIUS, 0},
             {17, simulation::MINE_RADIUS, 12}}};
        for (const auto position : POSITIONS) {
            m_mines.push_back({static_cast<std::uint32_t>(m_mines.size() + 1), position, 0});
        }
        for (std::size_t index = 0; index < simulation::INITIAL_COLLECTIBLES; ++index) {
            SpawnFood();
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
        player.body.position = {
            simulation::spawn_x(static_cast<std::uint32_t>(player.spawnSlot + 1)),
            simulation::PLAYER_RADIUS, 0};
        player.body.orientation = Quaternion::FromRotation({0, std::numbers::pi, 0});
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

    std::size_t CollectibleCount() const
    {
        return m_food.size();
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

    void AdvanceClock()
    {
        ++m_tick;
    }

    void Step()
    {
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
            player.previousPosition = player.body.position;
            if (m_tick - player.inputTick >= simulation::INPUT_TIMEOUT_TICKS) {
                player.input.x = player.input.z = 0;
            }
            Move(player);
        }

        ConsumeFood();
        TriggerMines();
        ResolvePlayers();

        ++m_tick;
        if (++m_spawnTicks % simulation::COLLECTIBLE_SPAWN_INTERVAL_TICKS == 0) {
            SpawnFood();
        }
    }

    WorldSnapshot Snapshot() const
    {
        WorldSnapshot result{m_tick, {}, m_food, {}, m_mines};
        for (const auto& [id, player] : m_players) {
            const auto& body = player.body;
            result.players.push_back({id,
                                      body.position,
                                      body.velocity,
                                      body.orientation,
                                      {},
                                      {},
                                      player.deathTick,
                                      0,
                                      false,
                                      body.radius,
                                      player.foodEaten,
                                      player.mineHitTick});
        }
        return result;
    }

  private:
    struct Player
    {
        RigidBody body;
        Vector3 previousPosition{};
        std::size_t spawnSlot{};
        InputCommand input;
        std::uint64_t inputTick{};
        std::uint64_t deathTick{};
        std::uint64_t mineHitTick{};
        std::uint32_t foodEaten{};
    };

    static double Volume(double radius)
    {
        return radius * radius * radius;
    }

    static void SetVolume(Player& player, double volume)
    {
        auto& body = player.body;
        body.radius =
            std::clamp(std::cbrt(volume), simulation::PLAYER_RADIUS, simulation::MAX_PLAYER_RADIUS);
        body.mass = simulation::PLAYER_MASS * Volume(body.radius / simulation::PLAYER_RADIUS);
        Constrain(player);
    }

    static void Constrain(Player& player)
    {
        auto& body = player.body;
        body.position.y = body.radius;
        const double limit = simulation::MAP_HALF_EXTENT - body.radius;
        body.position.x = std::clamp(body.position.x, -limit, limit);
        body.position.z = std::clamp(body.position.z, -limit, limit);
        // At 60 Hz movement is at most 0.1 m, far below the smallest rock's diameter.
        // Reapply after growth and player separation as well as normal movement.
        for (const auto& rock : ARENA_ROCKS) {
            Vector3 offset = body.position - rock.position;
            offset.y = 0;
            const double distance = offset.Length();
            const double separation = body.radius + rock.radius;
            if (distance >= separation) {
                continue;
            }
            const Vector3 normal = distance > 1e-9 ? offset * (1 / distance) : Vector3{1, 0, 0};
            body.position = body.position + normal * (separation - distance);
            const double inwardSpeed = dot(body.velocity, normal);
            if (inwardSpeed < 0) {
                body.velocity = body.velocity - normal * inwardSpeed;
            }
        }
    }

    static void Move(Player& player)
    {
        auto& body = player.body;
        const double terrainSpeed = in_mud(body.position) ? MUD_SPEED_FACTOR : 1.0;
        const double speed = simulation::PLAYER_MOVE_SPEED * terrainSpeed /
                             std::pow(body.radius / simulation::PLAYER_RADIUS, 0.25);
        Vector3 direction{player.input.x, 0, player.input.z};
        if (direction.Length() > 1) {
            direction = direction.Normalized();
        }
        const Vector3 target = direction * speed;
        body.velocity = body.velocity + (target - body.velocity) *
                                            (simulation::PLAYER_RESPONSE * simulation::FIXED_DELTA);
        if (body.velocity.Length() < 0.001 && direction.Length() == 0) {
            body.velocity = {};
        }
        body.position = body.position + body.velocity * simulation::FIXED_DELTA;
        if (direction.Length() > 0) {
            body.orientation =
                Quaternion::FromRotation({0, std::atan2(-direction.x, -direction.z), 0});
        }
        Constrain(player);
    }

    static double SweepFraction(Vector3 from, Vector3 to, Vector3 center, double radius)
    {
        from.y = to.y = center.y = 0;
        const auto offset = from - center;
        const auto movement = to - from;
        const double c = dot(offset, offset) - radius * radius;
        if (c <= 0) {
            return 0;
        }
        const double a = dot(movement, movement);
        const double b = dot(offset, movement);
        const double discriminant = b * b - a * c;
        if (a < 1e-12 || discriminant < 0) {
            return 2;
        }
        const double fraction = (-b - std::sqrt(discriminant)) / a;
        return fraction >= 0 && fraction <= 1 ? fraction : 2;
    }

    void ConsumeFood()
    {
        std::erase_if(m_food, [&](const CollectibleState& food) {
            Player* eater = nullptr;
            double firstHit = 2;
            const double radius = simulation::COLLECTIBLE_BASE_RADIUS * food.scale;
            for (auto& [id, player] : m_players) {
                (void)id;
                if (player.deathTick != 0 || player.body.radius < radius) {
                    continue;
                }
                const double hit = SweepFraction(player.previousPosition, player.body.position,
                                                 food.position, player.body.radius + radius);
                if (hit < firstHit) {
                    eater = &player;
                    firstHit = hit;
                }
            }
            if (!eater) {
                return false;
            }
            SetVolume(*eater, Volume(eater->body.radius) + Volume(radius));
            eater->foodEaten = std::min(eater->foodEaten + 1, simulation::MAX_FOOD_PER_ROUND);
            return true;
        });
    }

    void TriggerMines()
    {
        for (auto& mine : m_mines) {
            if (mine.readyTick > m_tick) {
                continue;
            }
            for (auto& [id, player] : m_players) {
                (void)id;
                if (player.deathTick != 0 || player.body.radius < simulation::MINE_TRIGGER_RADIUS ||
                    (player.mineHitTick != 0 &&
                     m_tick + 1 - player.mineHitTick < simulation::MINE_IMMUNITY_TICKS)) {
                    continue;
                }
                if (SweepFraction(player.previousPosition, player.body.position, mine.position,
                                  player.body.radius + simulation::MINE_RADIUS) > 1) {
                    continue;
                }
                SetVolume(player, Volume(player.body.radius) * simulation::MINE_RETAINED_VOLUME);
                player.mineHitTick = m_tick + 1;
                mine.readyTick = m_tick + 1 + simulation::MINE_RESPAWN_TICKS;
                break;
            }
        }
    }

    void ResolvePlayers()
    {
        // Resolve simultaneous eating from pre-collision sizes. A defeated eater gains nothing.
        std::map<std::uint32_t, std::uint32_t> eaters;
        for (auto first = m_players.begin(); first != m_players.end(); ++first) {
            for (auto second = std::next(first); second != m_players.end(); ++second) {
                auto& a = first->second;
                auto& b = second->second;
                if (a.deathTick != 0 || b.deathTick != 0 ||
                    SweepFraction(a.previousPosition - b.previousPosition,
                                  a.body.position - b.body.position, {},
                                  a.body.radius + b.body.radius) > 1) {
                    continue;
                }
                auto larger = a.body.radius >= b.body.radius ? first : second;
                auto smaller = larger == first ? second : first;
                if (larger->second.body.radius <= smaller->second.body.radius ||
                    Volume(larger->second.body.radius) <
                        Volume(smaller->second.body.radius) * simulation::EAT_VOLUME_RATIO) {
                    continue;
                }
                const auto victimId = smaller->first;
                if (!eaters.contains(victimId) ||
                    larger->second.body.radius > m_players.at(eaters.at(victimId)).body.radius) {
                    eaters[victimId] = larger->first;
                }
            }
        }
        std::map<std::uint32_t, double> gains;
        for (const auto& [victimId, eaterId] : eaters) {
            auto& victim = m_players.at(victimId);
            if (!eaters.contains(eaterId)) {
                gains[eaterId] += Volume(victim.body.radius);
            }
            victim.deathTick = m_tick + 1;
            victim.body.velocity = {};
            victim.input.x = victim.input.z = 0;
            victim.input.restartDeathTick = 0;
        }
        for (const auto& [id, volume] : gains) {
            auto& player = m_players.at(id);
            SetVolume(player, Volume(player.body.radius) + volume);
        }
        for (int iteration = 0; iteration < 4; ++iteration) {
            for (auto first = m_players.begin(); first != m_players.end(); ++first) {
                for (auto second = std::next(first); second != m_players.end(); ++second) {
                    if (first->second.deathTick != 0 || second->second.deathTick != 0) {
                        continue;
                    }
                    auto& a = first->second.body;
                    auto& b = second->second.body;
                    a.position.y = b.position.y = 0;
                    resolve_sphere_collision(a, b);
                    a.position.y = a.radius;
                    b.position.y = b.radius;
                }
                Constrain(first->second);
            }
        }
    }

    bool TryRestart(std::uint32_t id, Player& player)
    {
        for (std::size_t attempt = 0; attempt < 81; ++attempt) {
            const auto slot = (attempt + player.spawnSlot) % 81;
            const auto row = slot / 9;
            Vector3 position{-40 + static_cast<double>(slot % 9) * 10, simulation::PLAYER_RADIUS,
                             -40 + static_cast<double>(row) * 10};
            const bool safe = std::ranges::all_of(m_players, [&](const auto& entry) {
                const auto& other = entry.second;
                return other.deathTick != 0 ||
                       std::hypot(position.x - other.body.position.x,
                                  position.z - other.body.position.z) >
                           other.body.radius + simulation::PLAYER_RADIUS + 2;
            });
            if (!safe || overlaps_rock(position, simulation::PLAYER_RADIUS + 1) ||
                in_mud(position) || std::ranges::any_of(m_food, [&](const auto& food) {
                    return (food.position - position).Length() <
                           simulation::PLAYER_RADIUS +
                               simulation::COLLECTIBLE_BASE_RADIUS * food.scale + 1;
                })) {
                continue;
            }
            player.body = {};
            player.body.position = position;
            player.deathTick = 0;
            player.mineHitTick = 0;
            player.input = {id, player.input.sequence, 0, 0};
            player.inputTick = m_tick;
            return true;
        }
        return false;
    }

    bool SpawnFood()
    {
        if (m_food.size() >= MAX_COLLECTIBLES) {
            return false;
        }
        CollectibleState food;
        food.id = m_nextFoodId;
        food.kind = static_cast<CollectibleKind>(1 + m_random.Index(4));
        food.scale =
            m_random.Range(simulation::COLLECTIBLE_MIN_SCALE, simulation::COLLECTIBLE_MAX_SCALE);
        const double radius = simulation::COLLECTIBLE_BASE_RADIUS * food.scale;
        food.orientation =
            Quaternion::FromRotation({0, m_random.Range(0, 2 * std::numbers::pi), 0});
        for (std::size_t attempt = 0; attempt < simulation::COLLECTIBLE_SPAWN_ATTEMPTS; ++attempt) {
            food.position = {m_random.Range(-simulation::COLLECTIBLE_AREA_HALF_EXTENT,
                                            simulation::COLLECTIBLE_AREA_HALF_EXTENT),
                             radius,
                             m_random.Range(-simulation::COLLECTIBLE_AREA_HALF_EXTENT,
                                            simulation::COLLECTIBLE_AREA_HALF_EXTENT)};
            // Leave the initial player lane clear, and keep food out of mines and living players.
            if (food.position.x > -2 && food.position.x < 13 && std::abs(food.position.z) < 2) {
                continue;
            }
            if (overlaps_rock(food.position, radius + 0.5) ||
                std::ranges::any_of(
                    m_food,
                    [&](const auto& other) {
                        return (food.position - other.position).Length() <
                               radius + simulation::COLLECTIBLE_BASE_RADIUS * other.scale + 0.3;
                    }) ||
                std::ranges::any_of(m_mines,
                                    [&](const auto& mine) {
                                        return (food.position - mine.position).Length() <
                                               radius + simulation::MINE_RADIUS + 1;
                                    }) ||
                std::ranges::any_of(m_players, [&](const auto& entry) {
                    return entry.second.deathTick == 0 &&
                           (food.position - entry.second.body.position).Length() <
                               radius + entry.second.body.radius + 0.5;
                })) {
                continue;
            }
            m_food.push_back(food);
            ++m_nextFoodId;
            return true;
        }
        return false;
    }

    std::map<std::uint32_t, Player> m_players;
    std::array<bool, MAX_PLAYERS> m_spawnSlots{};
    std::vector<CollectibleState> m_food;
    std::vector<MineState> m_mines;
    DeterministicRandom m_random;
    std::uint32_t m_nextFoodId{1};
    std::uint64_t m_tick{};
    std::uint64_t m_spawnTicks{};
};

} // namespace MoongTangE
