#include "Client/Rendering/SnapshotInterpolator.h"
#include "TestSupport.h"

int main()
{
    return MoongTangE::test::run([] {
        using namespace MoongTangE;

        SnapshotInterpolator view;
        WorldSnapshot a{3, {{1, {0, 0.5, 0}, {}}}}, b = a;
        b.tick = 6;
        b.players[0].position.x = 1;
        b.players[0].orientation = Quaternion::FromRotation({0, 0, -2});

        view.Push(a);
        view.Push(b);
        view.Advance(0.025);
        auto middle = view.Sample();
        MOONGTANGE_CHECK(MoongTangE::test::approx(middle.players[0].position.x, 0.5));
        const auto expected = Quaternion::FromRotation({0, 0, -1});
        MOONGTANGE_CHECK(
            (middle.players[0].orientation.Rotate({1, 0, 0}) - expected.Rotate({1, 0, 0}))
                .Length() < 1e-9);
        MOONGTANGE_CHECK(a.players[0].position.x == 0 && b.players[0].position.x == 1);

        view.Push(a); // Stale packet cannot rewind the view.
        MOONGTANGE_CHECK(view.Sample().players[0].position == middle.players[0].position);

        WorldSnapshot c = b;
        c.tick = 9;
        c.players[0].position.x = 2;
        view.Push(c);
        MOONGTANGE_CHECK(view.Sample().players[0].position == middle.players[0].position);
        view.Advance(10);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 2);
        view.Advance(10);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 2); // No extrapolation.

        c.tick = 12;
        c.players = {{2, {5, 0.5, 0}, {}}};
        view.Push(c);
        MOONGTANGE_CHECK(view.Sample().players.size() == 1 && view.Sample().players[0].id == 2);
        MOONGTANGE_CHECK(view.Sample().players[0].position.x == 5);
    });
}
