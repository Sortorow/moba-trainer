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
    /// 効果を付ける。同じ種類（印は同じ tag）が既にあれば、残り時間と強さの大きい方に更新する
    /// @param source 効果を付けたキャラ
    void apply(const EffectSpec& spec, EntityId source);
    /// 残り時間を減らし、時間切れの効果を消す
    void update(float dt);
    /// すべての効果を消す
    void clear() { effects_.clear(); }

    /// その種類の効果がかかっているか
    bool has(EffectType type) const;
    /// その種類の効果の残り時間（同じ種類が複数あれば最も長いもの）。かかっていなければ 0
    float remaining(EffectType type) const;
    /// その名前の印が付いているか
    bool hasMark(std::string_view tag) const;
    /// 印を消費する（起爆した時など）
    /// @return 消費できたら true（その印が無ければ false）
    bool consumeMark(std::string_view tag);

    /// 移動できるか（拘束・スタン・打ち上げ中は移動できない）
    bool canMove() const;
    /// スキルを使えるか（スタン・打ち上げ・変身中は使えない）
    bool canCast() const;
    /// 打ち上げられているか（描く時に浮かせる）
    bool isAirborne() const { return has(EffectType::Knockup); }
    /// 移動速度に掛ける倍率（1 = そのまま）。スロウは重ねがけせず、最も強いものだけが効く（LoL と同じ）
    float moveSpeedMultiplier() const;

    /// かかっているすべての効果（画面の表示用）
    const std::vector<StatusEffect>& list() const { return effects_; }

private:
    std::vector<StatusEffect> effects_;
};

}  // namespace moba
