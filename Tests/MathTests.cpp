#include "Common/Math/Quaternion.h"
#include "Common/Math/Vector3.h"
#include "TestSupport.h"
#include <cmath>
#include <limits>
#include <numbers>

int main() {
    return MoongTangE::test::run([] {
        using MoongTangE::Vector3;
        MOONGTANGE_CHECK((Vector3{1, 2, 3} + Vector3{2, 3, 4}) == Vector3{3, 5, 7});
        MOONGTANGE_CHECK((Vector3{1, 2, 3} - Vector3{2, 3, 4}) == Vector3{-1, -1, -1});
        MOONGTANGE_CHECK(2.0 * Vector3{1, 2, 3} == Vector3{2, 4, 6});
        MOONGTANGE_CHECK(MoongTangE::test::approx(Vector3{3, 4, 0}.length(), 5));
        MOONGTANGE_CHECK(MoongTangE::test::approx(Vector3{3, 4, 0}.normalized().length(), 1));
        MOONGTANGE_CHECK(Vector3{}.normalized() == Vector3{});
        MOONGTANGE_CHECK(
            MoongTangE::test::approx(Vector3{1e308, 1e308, 0}.normalized().length(), 1));
        MOONGTANGE_CHECK(MoongTangE::test::approx(Vector3{1e-300, 0, 0}.normalized().x, 1));
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        MOONGTANGE_CHECK(!Vector3{nan, 0, 0}.finite());
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            (void)Vector3{nan, 0, 0}.normalized();
        }));
        MOONGTANGE_CHECK(MoongTangE::cross({1, 0, 0}, {0, 1, 0}) == Vector3{0, 0, 1});
        const auto q = MoongTangE::Quaternion::fromRotation({0, 0, std::numbers::pi / 2});
        MOONGTANGE_CHECK((q.rotate({1, 0, 0}) - Vector3{0, 1, 0}).length() < 1e-9);
        MOONGTANGE_CHECK(((q * q).rotate({1, 0, 0}) - Vector3{-1, 0, 0}).length() < 1e-9);
        const auto halfway = MoongTangE::interpolate({}, q, 0.5);
        MOONGTANGE_CHECK(MoongTangE::test::approx(halfway.rotate({1, 0, 0}).x, std::sqrt(0.5)));
        MOONGTANGE_CHECK(MoongTangE::interpolate(q, {-q.w, -q.x, -q.y, -q.z}, 0.5).unit());
        MOONGTANGE_CHECK(MoongTangE::test::throws([] {
            (void)MoongTangE::Quaternion{0, 0, 0, 0}.normalized();
        }));
    });
}
