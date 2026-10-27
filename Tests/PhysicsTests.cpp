#include "Common/Physics/Collision/SphereSphere.h"
#include "Common/Physics/CompoundBallCollider.h"
#include "Common/Physics/CompoundMassProperties.h"
#include "Common/Physics/PhysicsWorld.h"
#include "TestSupport.h"

#include <cmath>
#include <limits>
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

    first.position = {-0.6, 0.5, 0};
    second.position = {0.6, 0.5, 0};
    first.velocity = {};
    second.velocity = {};
    MOONGTANGE_CHECK(resolve_sphere_collision(first, second, 1.4));
    MOONGTANGE_CHECK((second.position - first.position).Length() >= 1.4);
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

void sphere_overlap_detects_contact()
{
    RigidBody first;
    RigidBody second;
    first.position = {0, 0.5, 0};
    second.position = {1, 0.5, 0};
    MOONGTANGE_CHECK(spheres_overlap(first, second));

    second.position.x = 1.000001;
    MOONGTANGE_CHECK(!spheres_overlap(first, second));
}

void compound_collider_uses_shape_proxies_and_surface_sweep()
{
    CompoundBallCollider collider(simulation::PLAYER_RADIUS);
    MOONGTANGE_CHECK(collider.Spheres().size() == 1);
    collider.AddCollectible({0, 0.75, 0}, {}, CollectibleKind::APPLE,
                            simulation::COLLECTIBLE_BASE_RADIUS);
    MOONGTANGE_CHECK(collider.Spheres().size() == 2);
    collider.AddCollectible({0.8, 0, 0}, {}, CollectibleKind::BANANA,
                            simulation::COLLECTIBLE_BASE_RADIUS);
    MOONGTANGE_CHECK(collider.Spheres().size() == 7);
    collider.AddCollectible({-0.8, 0, 0}, {}, CollectibleKind::BURGER,
                            simulation::COLLECTIBLE_BASE_RADIUS);
    MOONGTANGE_CHECK(collider.Spheres().size() == 10);
    collider.AddCollectible({0, 0, 0.8}, {}, CollectibleKind::DONUT,
                            simulation::COLLECTIBLE_BASE_RADIUS);
    MOONGTANGE_CHECK(collider.Spheres().size() == 14);

    const auto support = collider.Support({}, {0, 1, 0});
    MOONGTANGE_CHECK(
        MoongTangE::test::approx(support.y, 0.75 + simulation::COLLECTIBLE_BASE_RADIUS));

    SweptSphereContact contact;
    MOONGTANGE_CHECK(collider.SweepSphere({-5, 0, 0}, {5, 0, 0}, 0.2, contact));
    MOONGTANGE_CHECK(contact.fraction > 0 && contact.fraction < 0.5);
    MOONGTANGE_CHECK(contact.normal.x < -0.99);
    MOONGTANGE_CHECK(!collider.SweepSphere({-5, 3, 0}, {5, 3, 0}, 0.2, contact));
}

void compound_collider_resolves_deep_multi_contact_deterministically()
{
    const PhysicsWorld physics;
    CompoundBallCollider collider(0.5);
    collider.AddSphere({-0.4, -0.2, 0}, 0.3);
    collider.AddSphere({0.4, -0.2, 0}, 0.3);

    RigidBody first;
    RigidBody second;
    first.position = second.position = {0, -1, 0};
    first.velocity = second.velocity = {2, -20, 0};
    first.radius = second.radius = 0.5;
    for (int tick = 0; tick < 600; ++tick) {
        const Vector3 previousVelocity = first.velocity;
        physics.Step(first, collider);
        physics.Step(second, collider);
        const auto contacts = PhysicsWorld::TerrainContacts(first, collider);
        MOONGTANGE_CHECK(contacts.count >= 2);
        MOONGTANGE_CHECK(contacts.points[0].terrain.penetration <= 1e-9);
        MOONGTANGE_CHECK((first.velocity - previousVelocity).Length() < (tick < 60 ? 25 : 1));
        MOONGTANGE_CHECK(first.position == second.position && first.velocity == second.velocity &&
                         first.orientation == second.orientation &&
                         first.angularVelocity == second.angularVelocity);
    }

    RigidBody resting;
    resting.position = {0, 2, 0};
    for (int tick = 0; tick < 600; ++tick) {
        physics.Step(resting, collider);
    }
    MOONGTANGE_CHECK(resting.velocity == Vector3{} && resting.angularVelocity == Vector3{});
}

void compound_mass_properties_include_parallel_axis_inertia()
{
    CompoundMassProperties properties;
    properties.AddSphere(1, 0.5);
    properties.AddSphere(0.25, 0.35, {0, 0.75, 0});
    const double expected = 0.4 * 1 * 0.5 * 0.5 + 0.4 * 0.25 * 0.35 * 0.35 + 0.25 * 0.75 * 0.75;
    MOONGTANGE_CHECK(MoongTangE::test::approx(properties.Mass(), 1.25));
    MOONGTANGE_CHECK(MoongTangE::test::approx(properties.Inertia(), expected));
}

void invalid_compound_collider_is_rejected()
{
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        CompoundBallCollider invalid(0);
    }));

    CompoundBallCollider collider(simulation::PLAYER_RADIUS);
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        collider.AddSphere({}, 0);
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        SweptSphereContact contact;
        collider.SweepSphere({std::numeric_limits<double>::max(), 0, 0}, {}, 0.1, contact);
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
        sphere_overlap_detects_contact();
        compound_collider_uses_shape_proxies_and_surface_sweep();
        compound_collider_resolves_deep_multi_contact_deterministically();
        compound_mass_properties_include_parallel_axis_inertia();
        invalid_compound_collider_is_rejected();
    });
}
