#include "Common/Math/Quaternion.h"
#include "Common/Math/Vector3.h"
#include "TestSupport.h"

#include <cmath>
#include <limits>
#include <numbers>

int main()
{
    return MoongTangE::test::run([] {
        using MoongTangE::Vector3;

        MOONGTANGE_CHECK((Vector3{1, 2, 3} + Vector3{2, 3, 4}) == Vector3{3, 5, 7});
        MOONGTANGE_CHECK((Vector3{1, 2, 3} - Vector3{2, 3, 4}) == Vector3{-1, -1, -1});
        MOONGTANGE_CHECK(2.0 * Vector3{1, 2, 3} == Vector3{2, 4, 6});

        MOONGTANGE_CHECK(MoongTangE::test::approx(Vector3{3, 4, 0}.Length(), 5));
        MOONGTANGE_CHECK(MoongTangE::test::approx(Vector3{3, 4, 0}.Normalized().Length(), 1));
        MOONGTANGE_CHECK(Vector3{}.Normalized() == Vector3{});
        MOONGTANGE_CHECK(
            MoongTangE::test::approx(Vector3{1e308, 1e308, 0}.Normalized().Length(), 1));
        MOONGTANGE_CHECK(MoongTangE::test::approx(Vector3{1e-300, 0, 0}.Normalized().x, 1));

        const auto nan = std::numeric_limits<double>::quiet_NaN();
        MOONGTANGE_CHECK(!Vector3{nan, 0, 0}.Finite());
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            (void)Vector3{nan, 0, 0}.Normalized();
        }));

        MOONGTANGE_CHECK(MoongTangE::cross({1, 0, 0}, {0, 1, 0}) == Vector3{0, 0, 1});

        const auto q = MoongTangE::Quaternion::FromRotation({0, 0, std::numbers::pi / 2});
        MOONGTANGE_CHECK((q.Rotate({1, 0, 0}) - Vector3{0, 1, 0}).Length() < 1e-9);
        MOONGTANGE_CHECK(((q * q).Rotate({1, 0, 0}) - Vector3{-1, 0, 0}).Length() < 1e-9);
        MOONGTANGE_CHECK((q.Conjugate().Rotate(q.Rotate({1, 2, 3})) - Vector3{1, 2, 3}).Length() <
                         1e-9);

        const auto halfway = MoongTangE::interpolate({}, q, 0.5);
        MOONGTANGE_CHECK(MoongTangE::test::approx(halfway.Rotate({1, 0, 0}).x, std::sqrt(0.5)));
        MOONGTANGE_CHECK(MoongTangE::interpolate(q, {-q.w, -q.x, -q.y, -q.z}, 0.5).Unit());
        MOONGTANGE_CHECK(MoongTangE::test::throws([] {
            (void)MoongTangE::Quaternion{0, 0, 0, 0}.Normalized();
        }));
    });
}
