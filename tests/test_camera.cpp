// カメラの動かし方のテスト（自キャラに固定 / 固定しない）

#include <doctest/doctest.h>

#include "render/CameraControl.h"

using namespace moba;

namespace {

constexpr Rectangle kBounds{-2000.0f, -2000.0f, 4000.0f, 4000.0f};

// 1600x900 の画面で、端の判定の幅が 12 ピクセル
CameraControlInput inputAt(Vector2 mouse) {
    CameraControlInput input;
    input.mouse = mouse;
    input.screenWidth = 1600.0f;
    input.screenHeight = 900.0f;
    input.edgeMargin = 12.0f;
    input.edgeScrollEnabled = true;
    return input;
}

}  // namespace

TEST_CASE("カメラ「自キャラに固定」: 常に自キャラを映す") {
    const Vector2 target =
        nextCameraTarget(moba::CameraMode::Locked, {0, 0}, {300, 200}, inputAt({800, 450}), 2500.0f, kBounds, 0.1f);
    CHECK(target.x == doctest::Approx(300.0f));
    CHECK(target.y == doctest::Approx(200.0f));
}

TEST_CASE("カメラ「固定しない」: 自キャラが動いてもカメラは動かない") {
    const Vector2 target =
        nextCameraTarget(moba::CameraMode::Free, {0, 0}, {300, 200}, inputAt({800, 450}), 2500.0f, kBounds, 0.1f);
    CHECK(target.x == doctest::Approx(0.0f));
    CHECK(target.y == doctest::Approx(0.0f));
}

TEST_CASE("カメラ「固定しない」: 画面の端にカーソルを置くと、その方向へ動く") {
    // 右端: 0.1 秒で 2500 × 0.1 = 250 右へ
    const Vector2 right =
        nextCameraTarget(moba::CameraMode::Free, {0, 0}, {0, 0}, inputAt({1599, 450}), 2500.0f, kBounds, 0.1f);
    CHECK(right.x == doctest::Approx(250.0f));
    CHECK(right.y == doctest::Approx(0.0f));

    // 上端（端から 12 ピクセル以内なら動く）
    const Vector2 up =
        nextCameraTarget(moba::CameraMode::Free, {0, 0}, {0, 0}, inputAt({800, 10}), 2500.0f, kBounds, 0.1f);
    CHECK(up.y == doctest::Approx(-250.0f));

    // 端から 12 ピクセルより内側では動かない
    const Vector2 inside =
        nextCameraTarget(moba::CameraMode::Free, {0, 0}, {0, 0}, inputAt({800, 20}), 2500.0f, kBounds, 0.1f);
    CHECK(inside.y == doctest::Approx(0.0f));

    // 左下の角では斜めに動く（速さは同じ）
    const Vector2 dir = edgeScrollDirection(inputAt({0, 899}));
    CHECK(dir.x < 0.0f);
    CHECK(dir.y > 0.0f);
    CHECK(dir.x * dir.x + dir.y * dir.y == doctest::Approx(1.0f));
}

TEST_CASE("カメラ「固定しない」: ステージの外までは動かない") {
    const Vector2 target =
        nextCameraTarget(moba::CameraMode::Free, {1950, 0}, {0, 0}, inputAt({1599, 450}), 2500.0f, kBounds, 0.1f);
    CHECK(target.x == doctest::Approx(2000.0f));
}

TEST_CASE("カメラ「固定しない」: Space を押している間は自キャラを映す。UI の上では端でも動かない") {
    CameraControlInput held = inputAt({1599, 450});
    held.centerHeld = true;
    const Vector2 centered = nextCameraTarget(moba::CameraMode::Free, {0, 0}, {300, 200}, held, 2500.0f, kBounds, 0.1f);
    CHECK(centered.x == doctest::Approx(300.0f));
    CHECK(centered.y == doctest::Approx(200.0f));

    CameraControlInput onUi = inputAt({1599, 450});
    onUi.edgeScrollEnabled = false;
    const Vector2 still = nextCameraTarget(moba::CameraMode::Free, {0, 0}, {300, 200}, onUi, 2500.0f, kBounds, 0.1f);
    CHECK(still.x == doctest::Approx(0.0f));
}
