#pragma once
// キャラにかかっている状態異常（スロウ・拘束・変身など）と印

#include <string>
#include <string_view>
#include <vector>

#include "core/GameTypes.h"
#include "data/EffectSpec.h"

namespace moba {

struct StatusEffect {
    EffectType type = EffectType::Slow;
    float remaining = 0.0f;  // 残り時間
    float duration = 0.0f;   // 付与された時の長さ
    float amount = 0.0f;
    std::string tag;
    EntityId source = kInvalidId;  // 付与したキャラ
};

class StatusEffects {
public:
    // 効果を付ける。同じ種類（印は同じ tag）が既にあれば、残り時間と強さの大きい方に更新する
    void apply(const EffectSpec& spec, EntityId source);
    void update(float dt);
    void clear() { effects_.clear(); }

    bool has(EffectType type) const;
    float remaining(EffectType type) const;  // 同じ種類が複数あれば最も長い残り時間
    bool hasMark(std::string_view tag) const;
    bool consumeMark(std::string_view tag);  // 印を消費する。消費できたら true

    bool canMove() const;  // 拘束・スタン・打ち上げ中は移動できない
    bool canCast() const;  // スタン・打ち上げ・変身中はスキルを使えない
    bool isAirborne() const { return has(EffectType::Knockup); }
    float moveSpeedMultiplier() const;  // スロウは重ねがけせず、最も強いものだけが効く（LoL と同じ）

    const std::vector<StatusEffect>& list() const { return effects_; }

private:
    std::vector<StatusEffect> effects_;
};

}  // namespace moba
