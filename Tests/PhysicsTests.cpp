#include "Common/Physics/PhysicsWorld.h"
#include "TestSupport.h"
#include <cmath>
#include <numbers>

namespace {
using namespace MoongTangE;

double kineticEnergy(const RigidBody& body) {
    return 0.5 * body.mass * dot(body.velocity, body.velocity) +
           0.5 * body.inertia() * dot(body.angularVelocity, body.angularVelocity);
}

void gravityAndGroundContact() {
    PhysicsWorld physics;
    RigidBody body;
    physics.step(body);
    MOONGTANGE_CHECK(MoongTangE::test::approx(body.velocity.y, -9.81 / 60));
    MOONGTANGE_CHECK(MoongTangE::test::approx(body.position.y, 2 - 9.81 / 3600));
    for (int tick = 0; tick < 3600; ++tick) {
        physics.step(body);
        MOONGTANGE_CHECK(body.position.y >= body.radius);
    }
    MOONGTANGE_CHECK(body.position.y == 0.5 && body.velocity.y == 0);
    body.position.y = -10;
    body.velocity.y = -1000;
    physics.step(body);
    MOONGTANGE_CHECK(body.position.y == 0.5 && body.velocity.y == 0);
}

void rollingDistanceMatchesRotation() {
    const PhysicsWorld noResistance({0.8, 0, 0});
    for (const double radius : {0.25, 0.5, 1.0}) {
        RigidBody rolling;
        rolling.radius = radius;
        rolling.position = {0, radius, 0};
        rolling.velocity = {std::numbers::pi * radius, 0, 0};
        rolling.angularVelocity = {0, 0, -std::numbers::pi};
        for (int tick = 0; tick < 120; ++tick) {
            noResistance.step(rolling);
            const auto expected = Quaternion::fromRotation({0, 0, -rolling.position.x / radius});
            MOONGTANGE_CHECK(
                (rolling.orientation.rotate({1, 0, 0}) - expected.rotate({1, 0, 0})).length() <
                1e-9);
        }
        MOONGTANGE_CHECK(
            MoongTangE::test::approx(rolling.position.x, 2 * std::numbers::pi * radius));
        MOONGTANGE_CHECK(MoongTangE::test::approx(std::abs(rolling.orientation.w), 1));
    }
}

void slidingConvertsToRollingAndCoastsToRest() {
    const PhysicsWorld noResistance({0.8, 0, 0});
    RigidBody sliding;
    sliding.position = {0, 0.5, 0};
    sliding.velocity = {3, 0, 0};
    double previousEnergy = kineticEnergy(sliding);
    for (int tick = 0; tick < 120; ++tick) {
        noResistance.step(sliding);
        MOONGTANGE_CHECK(kineticEnergy(sliding) <= previousEnergy + 1e-10);
        previousEnergy = kineticEnergy(sliding);
    }
    MOONGTANGE_CHECK(MoongTangE::test::approx(sliding.velocity.x, 15.0 / 7));
    MOONGTANGE_CHECK(
        MoongTangE::test::approx(sliding.angularVelocity.z, -sliding.velocity.x / sliding.radius));

    const PhysicsWorld physics;
    const double start = sliding.position.x;
    physics.step(sliding);
    MOONGTANGE_CHECK(sliding.velocity.x > 0 && sliding.position.x > start);
    for (int tick = 0; tick < 600; ++tick) {
        const double before = kineticEnergy(sliding);
        physics.step(sliding);
        MOONGTANGE_CHECK(kineticEnergy(sliding) <= before + 1e-10);
    }
    MOONGTANGE_CHECK(sliding.velocity == Vector3{} && sliding.angularVelocity == Vector3{});
}

void oppositeInputBrakesBeforeReversing() {
    const PhysicsWorld physics;
    RigidBody driven;
    driven.position = {0, 0.5, 0};
    for (int tick = 0; tick < 120; ++tick) {
        physics.step(driven, PhysicsWorld::fixedDelta, {10, 0, 0});
    }
    const double speed = driven.velocity.x;
    MOONGTANGE_CHECK(speed > 3 && speed < 5.5);
    physics.step(driven, PhysicsWorld::fixedDelta, {-10, 0, 0});
    MOONGTANGE_CHECK(driven.velocity.x > 0 && driven.velocity.x < speed);
    for (int tick = 0; tick < 120; ++tick) {
        physics.step(driven, PhysicsWorld::fixedDelta, {-10, 0, 0});
    }
    MOONGTANGE_CHECK(driven.velocity.x < 0);
}

void airborneSpinIgnoresGroundForces() {
    const PhysicsWorld physics;
    RigidBody airborne;
    airborne.position.y = 10000;
    airborne.velocity = {2, 0, 3};
    airborne.angularVelocity = {1, 2, 3};
    for (int tick = 0; tick < 600; ++tick) {
        physics.step(airborne, PhysicsWorld::fixedDelta, {10, 0, 0});
    }
    MOONGTANGE_CHECK(airborne.velocity.x == 2 && airborne.velocity.z == 3);
    MOONGTANGE_CHECK(airborne.angularVelocity == Vector3{1, 2, 3} && airborne.orientation.unit());
}

void repeatedSimulationKeepsNormalizedOrientation() {
    const PhysicsWorld physics;
    RigidBody first;
    RigidBody second;
    first.position = second.position = {0, 0.5, 0};
    for (int tick = 0; tick < 36000; ++tick) {
        physics.step(first, PhysicsWorld::fixedDelta, {6, 0, 8});
        physics.step(second, PhysicsWorld::fixedDelta, {6, 0, 8});
        MOONGTANGE_CHECK(first.orientation.unit());
    }
    MOONGTANGE_CHECK(first.position == second.position && first.orientation == second.orientation &&
                     first.angularVelocity == second.angularVelocity);
}

void invalidStateIsRejected() {
    const PhysicsWorld physics;
    RigidBody body;
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        physics.step(body, 0);
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        physics.step(body, -1);
    }));
    body.orientation = {0, 0, 0, 0};
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        physics.step(body);
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        PhysicsWorld invalid({-1, 0, 0});
    }));
}
} // namespace

int main() {
    return MoongTangE::test::run([] {
        gravityAndGroundContact();
        rollingDistanceMatchesRotation();
        slidingConvertsToRollingAndCoastsToRest();
        oppositeInputBrakesBeforeReversing();
        airborneSpinIgnoresGroundForces();
        repeatedSimulationKeepsNormalizedOrientation();
        invalidStateIsRejected();
    });
}