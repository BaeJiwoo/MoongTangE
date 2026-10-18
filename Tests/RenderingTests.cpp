#include "Client/Rendering/SnapshotInterpolator.h"
#include "TestSupport.h"

int main() {
    return MoongTangE::test::run([] {
        using namespace MoongTangE;
        SnapshotInterpolator view;
        WorldSnapshot a{3, {{1, {0, 0.5, 0}, {}}}}, b = a;
        b.tick = 6;
        b.players[0].position.x = 1;
        b.players[0].orientation = Quaternion::fromRotation({0, 0, -2});
        view.push(a);
        view.push(b);
        view.advance(0.025);
        auto middle = view.sample();
        MOONGTANGE_CHECK(MoongTangE::test::approx(middle.players[0].position.x, 0.5));
        const auto expected = Quaternion::fromRotation({0, 0, -1});
        MOONGTANGE_CHECK(
            (middle.players[0].orientation.rotate({1, 0, 0}) - expected.rotate({1, 0, 0}))
                .length() < 1e-9);
        MOONGTANGE_CHECK(a.players[0].position.x == 0 && b.players[0].position.x == 1);
        view.push(a); // Stale packet cannot rewind the view.
        MOONGTANGE_CHECK(view.sample().players[0].position == middle.players[0].position);
        WorldSnapshot c = b;
        c.tick = 9;
        c.players[0].position.x = 2;
        view.push(c);
        MOONGTANGE_CHECK(view.sample().players[0].position == middle.players[0].position);
        view.advance(10);
        MOONGTANGE_CHECK(view.sample().players[0].position.x == 2);
        view.advance(10);
        MOONGTANGE_CHECK(view.sample().players[0].position.x == 2); // No extrapolation.
        c.tick = 12;
        c.players = {{2, {5, 0.5, 0}, {}}};
        view.push(c);
        MOONGTANGE_CHECK(view.sample().players.size() == 1 && view.sample().players[0].id == 2);
        MOONGTANGE_CHECK(view.sample().players[0].position.x == 5);
    });
}
