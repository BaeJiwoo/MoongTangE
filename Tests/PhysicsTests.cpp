#include "Common/Physics/Collision/SphereSphere.h"
#include "Common/Physics/PhysicsWorld.h"
#include "TestSupport.h"

#include <cmath>
#include <numbers>

namespace {

using namespace MoongTangE;

double kinetic_energy(const RigidBody& body)
{
    return 0.5 * body.mass * dot(body.velocity, body.velocity) +
           0.5 * body.Inertia() * dot(body.angularVelocity, body.angularVelocity);
}

void gravity_and_ground_contact()
{
    PhysicsWorld physics;
    RigidBody body;
    physics.Step(body);
    MOONGTANGE_CHECK(MoongTangE::test::approx(body.velocity.y, -9.81 / 60));
    MOONGTANGE_CHECK(MoongTangE::test::approx(body.position.y, 2 - 9.81 / 3600));

    for (int tick = 0; tick < 3600; ++tick) {
        physics.Step(body);
        MOONGTANGE_CHECK(body.position.y >= body.radius);
    }
    MOONGTANGE_CHECK(body.position.y == 0.5 && body.velocity.y == 0);

    body.position.y = -10;
    body.velocity.y = -1000;
    physics.Step(body);
    MOONGTANGE_CHECK(body.position.y == 0.5 && body.velocity.y == 0);
}

void varied_terrain_contacts()
{
    const auto flat = terrain_sample(20, 20);
    MOONGTANGE_CHECK(flat.height == 0 && flat.normal == Vector3{0, 1, 0});

    const auto pointed = terrain_sample(POINTED_TERRAIN.x, POINTED_TERRAIN.z);
    MOONGTANGE_CHECK(pointed.height == POINTED_TERRAIN.height);
    const auto hill = terrain_sample(HILL_TERRAIN[0].x, HILL_TERRAIN[0].z);
    MOONGTANGE_CHECK(hill.height == HILL_TERRAIN[0].radius + HILL_TERRAIN[0].centerY);

    PhysicsWorld physics;
    for (const auto position : {Vector3{POINTED_TERRAIN.x, 5, POINTED_TERRAIN.z},
                                Vector3{HILL_TERRAIN[0].x, 5, HILL_TERRAIN[0].z}}) {
        RigidBody body;
        body.position = position;
        for (int tick = 0; tick < 600; ++tick) {
            physics.Step(body);
            const auto terrain = terrain_sample(body.position.x, body.position.z);
            const double normalDistance = (body.position.y - terrain.height) * terrain.normal.y;
            MOONGTANGE_CHECK(normalDistance + 1e-9 >= body.radius);
        }
    }
}

void sphere_pair_collision_response()
{
    RigidBody first;
    RigidBody second;
    first.position = {-0.4, 0.5, 0};
    second.position = {0.4, 0.5, 0};
    first.velocity = {1, 0, 0};
    second.velocity = {-1, 0, 0};
    const double energyBefore = kinetic_energy(first) + kinetic_energy(second);

    MOONGTANGE_CHECK(resolve_sphere_collision(first, second));
    MOONGTANGE_CHECK((second.position - first.position).Length() >= first.radius + second.radius);
    MOONGTANGE_CHECK(first.velocity.x < 0 && second.velocity.x > 0);
    MOONGTANGE_CHECK(kinetic_energy(first) + kinetic_energy(second) <= energyBefore);

    first.position = {-0.4, 0.5, 0};
    second.position = {0.4, 0.5, 0};
    first.velocity = {-1, 0, 0};
    second.velocity = {1, 0, 0};
    MOONGTANGE_CHECK(resolve_sphere_collision(first, second));
    MOONGTANGE_CHECK(first.velocity == Vector3{-1, 0, 0});
    MOONGTANGE_CHECK(second.velocity == Vector3{1, 0, 0});
}

void rolling_distance_matches_rotation()
{
    const PhysicsWorld noResistance({0.8, 0, 0});
    for (const double radius : {0.25, 0.5, 1.0}) {
        RigidBody rolling;
        rolling.radius = radius;
        rolling.position = {0, radius, 0};
        rolling.velocity = {std::numbers::pi * radius, 0, 0};
        rolling.angularVelocity = {0, 0, -std::numbers::pi};

        for (int tick = 0; tick < 120; ++tick) {
            noResistance.Step(rolling);
            const auto expected = Quaternion::FromRotation({0, 0, -rolling.position.x / radius});
            MOONGTANGE_CHECK(
                (rolling.orientation.Rotate({1, 0, 0}) - expected.Rotate({1, 0, 0})).Length() <
                1e-9);
        }
        MOONGTANGE_CHECK(
            MoongTangE::test::approx(rolling.position.x, 2 * std::numbers::pi * radius));
        MOONGTANGE_CHECK(MoongTangE::test::approx(std::abs(rolling.orientation.w), 1));
    }
}

void sliding_converts_to_rolling_and_coasts_to_rest()
{
    const PhysicsWorld noResistance({0.8, 0, 0});
    RigidBody sliding;
    sliding.position = {0, 0.5, 0};
    sliding.velocity = {3, 0, 0};
    double previousEnergy = kinetic_energy(sliding);

    for (int tick = 0; tick < 120; ++tick) {
        noResistance.Step(sliding);
        MOONGTANGE_CHECK(kinetic_energy(sliding) <= previousEnergy + 1e-10);
        previousEnergy = kinetic_energy(sliding);
    }
    MOONGTANGE_CHECK(MoongTangE::test::approx(sliding.velocity.x, 15.0 / 7));
    MOONGTANGE_CHECK(
        MoongTangE::test::approx(sliding.angularVelocity.z, -sliding.velocity.x / sliding.radius));

    const PhysicsWorld physics;
    const double start = sliding.position.x;
    physics.Step(sliding);
    MOONGTANGE_CHECK(sliding.velocity.x > 0 && sliding.position.x > start);

    for (int tick = 0; tick < 600; ++tick) {
        const double before = kinetic_energy(sliding);
        physics.Step(sliding);
        MOONGTANGE_CHECK(kinetic_energy(sliding) <= before + 1e-10);
    }
    MOONGTANGE_CHECK(sliding.velocity == Vector3{} && sliding.angularVelocity == Vector3{});
}

void opposite_input_brakes_before_reversing()
{
    const PhysicsWorld physics;
    RigidBody driven;
    driven.position = {0, 0.5, 0};

    for (int tick = 0; tick < 120; ++tick) {
        physics.Step(driven, PhysicsWorld::FIXED_DELTA, {10, 0, 0});
    }
    const double speed = driven.velocity.x;
    MOONGTANGE_CHECK(speed > 3 && speed < 5.5);
    physics.Step(driven, PhysicsWorld::FIXED_DELTA, {-10, 0, 0});
    MOONGTANGE_CHECK(driven.velocity.x > 0 && driven.velocity.x < speed);

    for (int tick = 0; tick < 120; ++tick) {
        physics.Step(driven, PhysicsWorld::FIXED_DELTA, {-10, 0, 0});
    }
    MOONGTANGE_CHECK(driven.velocity.x < 0);
}

void airborne_spin_ignores_ground_forces()
{
    const PhysicsWorld physics;
    RigidBody airborne;
    airborne.position.y = 10000;
    airborne.velocity = {2, 0, 3};
    airborne.angularVelocity = {1, 2, 3};

    for (int tick = 0; tick < 600; ++tick) {
        physics.Step(airborne, PhysicsWorld::FIXED_DELTA, {10, 0, 0});
    }
    MOONGTANGE_CHECK(airborne.velocity.x == 2 && airborne.velocity.z == 3);
    MOONGTANGE_CHECK(airborne.angularVelocity == Vector3{1, 2, 3} && airborne.orientation.Unit());
}

void repeated_simulation_keeps_normalized_orientation()
{
    const PhysicsWorld physics;
    RigidBody first;
    RigidBody second;
    first.position = second.position = {0, 0.5, 0};

    for (int tick = 0; tick < 36000; ++tick) {
        physics.Step(first, PhysicsWorld::FIXED_DELTA, {6, 0, 8});
        physics.Step(second, PhysicsWorld::FIXED_DELTA, {6, 0, 8});
        MOONGTANGE_CHECK(first.orientation.Unit());
    }
    MOONGTANGE_CHECK(first.position == second.position && first.orientation == second.orientation &&
                     first.angularVelocity == second.angularVelocity);
}

void invalid_state_is_rejected()
{
    const PhysicsWorld physics;
    RigidBody body;
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        physics.Step(body, 0);
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        physics.Step(body, -1);
    }));

    body.orientation = {0, 0, 0, 0};
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        physics.Step(body);
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        PhysicsWorld invalid({-1, 0, 0});
    }));
}

} // namespace

int main()
{
    return MoongTangE::test::run([] {
        gravity_and_ground_contact();
        varied_terrain_contacts();
        sphere_pair_collision_response();
        rolling_distance_matches_rotation();
        sliding_converts_to_rolling_and_coasts_to_rest();
        opposite_input_brakes_before_reversing();
        airborne_spin_ignores_ground_forces();
        repeated_simulation_keeps_normalized_orientation();
        invalid_state_is_rejected();
    });
}
