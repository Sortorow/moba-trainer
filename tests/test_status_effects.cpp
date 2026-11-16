// 状態異常の一覧のテスト: スロウは強さごとに別々に数え、その時に残っている中で最も強いものだけが効く
// （ケース 1〜3 は character_traits.txt の「状態異常」の例 1〜3。ケース 3 は 2026-11-15 にユーザーが示した例。
//  ケース 1・2 は、鉄菱を「踏むたびにスロウ」にした時（2026-11-16）に、新しい鉄菱で書き直した）

#include <doctest/doctest.h>

#include <cmath>

#include "game/StatusEffects.h"

using namespace moba;

namespace {

constexpr float kDt = 1.0f / 120.0f;  // App::kFixedDt と同じ刻み

/// スロウの効果
EffectSpec slow(float amount, float duration) { return EffectSpec{EffectType::Slow, duration, amount, ""}; }

/// エマの Q の 2 枚当て（40%・1.5 秒。like_emma.jsonc の doubleHitEffects と同じ値）
const EffectSpec kQDoubleHit = slow(0.4f, 1.5f);
/// エマの D の鉄菱を 1 つ踏んだ時のスロウ（50%・2 秒。like_emma.jsonc の onStep と同じ値。踏むたびに付く）
const EffectSpec kCaltrops = slow(0.5f, 2.0f);

/// seconds 秒進める（World と同じく 1 刻みずつ、残り時間を減らす）
void advance(StatusEffects& status, float seconds) {
    const long ticks = std::lround(seconds / kDt);
    for (long i = 0; i < ticks; ++i) status.update(kDt);
}

}  // namespace

TEST_CASE(
    "スロウ: 弱い方が先に切れても、強い方が効いている間は変わらない（ケース 1: Q の 2 枚当ての 0.5 "
    "秒後に鉄菱を踏む）") {
    StatusEffects status;
    status.apply(kQDoubleHit, kInvalidId);
    advance(status, 0.25f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.6f));  // Q の 40% だけ
    advance(status, 0.25f);
    status.apply(kCaltrops, kInvalidId);  // 0.5 秒: 鉄菱を 1 つ踏む（2.5 秒まで）
    advance(status, 0.25f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 0.75 秒: 鉄菱の 50% が強い
    advance(status, 0.85f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 1.6 秒: Q は 1.5 秒で切れたが、鉄菱が残る
    CHECK(status.list().size() == 1);
    advance(status, 0.8f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 2.4 秒
    advance(status, 0.2f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 2.6 秒: 鉄菱も 2.5 秒で切れた
    CHECK(status.list().empty());
}

TEST_CASE("スロウ: 同じ強さのスロウを付け直すと、最後に付けた時から数え直す（ケース 2: 0.8 秒と 1.2 秒に鉄菱を踏む）") {
    StatusEffects status;
    advance(status, 0.8f);
    status.apply(kCaltrops, kInvalidId);  // 0.8 秒: 1 つ目を踏む（このままなら 2.8 秒まで）
    advance(status, 0.4f);
    status.apply(kCaltrops, kInvalidId);  // 1.2 秒: 2 つ目を踏む（3.2 秒まで）
    CHECK(status.list().size() == 1);     // 強さが同じなので、1 つにまとまる
    advance(status, 1.7f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 2.9 秒: 1 つ目だけなら切れている
    advance(status, 0.2f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 3.1 秒
    advance(status, 0.2f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 3.3 秒: 2 つ目から 2 秒で切れた
}

TEST_CASE("スロウ: 強さと長さの違う 2 つを同時に受けると、強い方が切れた後は弱い方が残りの長さだけ効く（ケース 3）") {
    StatusEffects status;
    status.apply(slow(0.5f, 2.0f), kInvalidId);
    status.apply(slow(0.3f, 4.0f), kInvalidId);
    advance(status, 1.0f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 1 秒: 50%
    advance(status, 2.0f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.7f));  // 3 秒: 50% は 2 秒で切れ、30%
    advance(status, 1.1f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 4.1 秒: どちらも切れた
}

TEST_CASE("スロウ: 同じ強さを付け直すと、別の効果にならず、残り時間の長い方になる") {
    StatusEffects status;
    status.apply(slow(0.5f, 1.0f), kInvalidId);
    advance(status, 0.5f);
    status.apply(slow(0.5f, 1.0f), kInvalidId);  // 残り 0.5 秒 → 1.0 秒に戻る
    REQUIRE(status.list().size() == 1);
    CHECK(status.list().front().remaining == doctest::Approx(1.0f));
    status.apply(slow(0.5f, 0.2f), kInvalidId);  // 短いものを付けても、残り時間は縮まない
    CHECK(status.list().front().remaining == doctest::Approx(1.0f));
}
