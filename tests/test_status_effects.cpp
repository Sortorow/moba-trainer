// 状態異常の一覧のテスト: スロウは強さごとに別々に数え、その時に残っている中で最も強いものだけが効く
// （ケース 1〜3 は、2026-11-15 にユーザーが示した例。character_traits.txt の「状態異常」）

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
/// エマの D の鉄菱（50%・出た後 0.5 秒。範囲の中にいる間は、毎刻み付け直す）
const EffectSpec kCaltrops = slow(0.5f, 0.5f);

/// seconds 秒進める。World と同じく 1 刻みずつ、残り時間を減らしてから、範囲の効果（zone があれば）を付け直す
void advance(StatusEffects& status, float seconds, const EffectSpec* zone = nullptr) {
    const long ticks = std::lround(seconds / kDt);
    for (long i = 0; i < ticks; ++i) {
        status.update(kDt);
        if (zone) status.apply(*zone, kInvalidId);
    }
}

}  // namespace

TEST_CASE("スロウ: 強い方が先に切れたら、残っている弱い方が効く（ケース 1: Q の 2 枚当ての 0.5 秒後に鉄菱に触れる）") {
    StatusEffects status;
    status.apply(kQDoubleHit, kInvalidId);
    advance(status, 0.25f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.6f));  // Q の 40% だけ
    advance(status, 0.25f);
    status.apply(kCaltrops, kInvalidId);  // 0.5 秒: 鉄菱に触れて、すぐ出る
    advance(status, 0.25f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 0.75 秒: 鉄菱の 50% が強い
    advance(status, 0.5f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.6f));  // 1.25 秒: 鉄菱は 1.0 秒で切れ、Q の 40% に戻る
    advance(status, 0.35f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 1.6 秒: Q も 1.5 秒で切れた
    CHECK(status.list().empty());
}

TEST_CASE("スロウ: 弱い方より長く続く強いスロウは、最後まで強いまま（ケース 2: 0.8 秒から 1.2 秒まで鉄菱の中）") {
    StatusEffects status;
    status.apply(kQDoubleHit, kInvalidId);
    advance(status, 0.5f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.6f));
    advance(status, 0.3f);
    status.apply(kCaltrops, kInvalidId);  // 0.8 秒: 鉄菱の範囲に入る
    advance(status, 0.2f, &kCaltrops);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 1.0 秒
    CHECK(status.list().size() == 2);   // 鉄菱を毎刻み付け直しても、強さが同じものは 1 つにまとまる
    advance(status, 0.2f, &kCaltrops);  // 1.2 秒: 範囲から出る（鉄菱のスロウは 1.7 秒まで）
    advance(status, 0.4f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(0.5f));  // 1.6 秒: Q は切れたが、鉄菱が残る
    advance(status, 0.2f);
    CHECK(status.moveSpeedMultiplier() == doctest::Approx(1.0f));  // 1.8 秒
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
