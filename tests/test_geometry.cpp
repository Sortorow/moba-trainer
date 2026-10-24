#include <doctest/doctest.h>

#include "core/Geometry.h"

using namespace moba;

TEST_CASE("点と線分の距離") {
    CHECK(geo::distanceSqToSegment({5, 3}, {0, 0}, {10, 0}) == doctest::Approx(9.0f));
    CHECK(geo::distanceSqToSegment({-4, 0}, {0, 0}, {10, 0}) == doctest::Approx(16.0f));  // 線分の端より外
    CHECK(geo::distanceSqToSegment({3, 4}, {0, 0}, {0, 0}) == doctest::Approx(25.0f));    // 長さ 0 の線分
}

TEST_CASE("動く円と円の当たり判定") {
    // 半径 10 の円が (0,0)→(100,0) に動く。中心 (50, 25)・半径 20 の円とは 30 以内なので当たる
    CHECK(geo::sweptCircleHitsCircle({0, 0}, {100, 0}, 10, {50, 25}, 20));
    CHECK_FALSE(geo::sweptCircleHitsCircle({0, 0}, {100, 0}, 10, {50, 31}, 20));
    // 1 刻みで大きく進んでも、間にある円をすり抜けない
    CHECK(geo::sweptCircleHitsCircle({0, 0}, {1000, 0}, 5, {500, 0}, 5));
}

TEST_CASE("射程で距離を制限する") {
    const Vector2 p = geo::clampDistance({0, 0}, {300, 400}, 250);
    CHECK(p.x == doctest::Approx(150.0f));
    CHECK(p.y == doctest::Approx(200.0f));
    const Vector2 inside = geo::clampDistance({0, 0}, {30, 40}, 250);
    CHECK(inside.x == doctest::Approx(30.0f));
}
