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

TEST_CASE("まっすぐ飛ぶ弾の当たり判定: 出た位置より後ろには当たらない") {
    // 半径 10 の弾が、出た位置 (0,0) から右へ (0,0)→(100,0) に動く。相手の円の半径は 20
    const Vector2 origin{0, 0};
    const Vector2 dir{1, 0};
    auto hits = [&](Vector2 center) {
        return geo::sweptCircleHitsCircleAhead(origin, dir, {0, 0}, {100, 0}, 10, center, 20);
    };
    CHECK(hits({50, 25}));        // 前（普通の判定と同じ）
    CHECK_FALSE(hits({50, 31}));  // 前でも、幅より外
    CHECK_FALSE(hits({-21, 0}));  // 円がすべて後ろ（普通の判定なら当たる距離）
    CHECK(geo::sweptCircleHitsCircle({0, 0}, {100, 0}, 10, {-21, 0}, 20));
    CHECK(hits({-12, 0}));  // 中心は後ろだが、円が出た位置の線より前に出ている
    // 中心が 12 後ろの円は、線の上で幅 ±16 だけ前に出ている。弾の半径 10 と合わせて、横に 26 までなら当たる
    CHECK(hits({-12, 25}));
    CHECK_FALSE(hits({-12, 27}));
    // 2 回目以降の刻み（出た位置より前を動いている時）は、普通の判定と同じ
    CHECK(geo::sweptCircleHitsCircleAhead(origin, dir, {100, 0}, {200, 0}, 10, {150, 29}, 20));
    CHECK_FALSE(geo::sweptCircleHitsCircleAhead(origin, dir, {100, 0}, {200, 0}, 10, {150, 31}, 20));
}

TEST_CASE("射程で距離を制限する") {
    const Vector2 p = geo::clampDistance({0, 0}, {300, 400}, 250);
    CHECK(p.x == doctest::Approx(150.0f));
    CHECK(p.y == doctest::Approx(200.0f));
    const Vector2 inside = geo::clampDistance({0, 0}, {30, 40}, 250);
    CHECK(inside.x == doctest::Approx(30.0f));
}
