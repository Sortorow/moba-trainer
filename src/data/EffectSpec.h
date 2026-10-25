#pragma once
// スキルが当たった相手に与える効果（設定ファイルの "onHit" などに書くもの）

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace moba {

/// スキルが当たった相手に与える効果の種類
enum class EffectType {
    Slow,       // 移動速度低下（amount = 低下率 0〜1）
    Snare,      // 移動不可（スキルは使える）
    Stun,       // 行動不能
    Knockup,    // 打ち上げ（行動不能）
    Polymorph,  // 変身。移動だけできる（amount = 移動速度の低下率）
    Mark,       // 印。tag で種類を区別する（エズリアルの W など）
};

/// 効果 1 つ（設定ファイルの { "type": ..., "duration": ... } の中身）
struct EffectSpec {
    EffectType type = EffectType::Slow;
    float duration = 0.0f;  // 秒
    float amount = 0.0f;    // 割合（0.4 = 40%）
    std::string tag;        // 印の名前
};

/// 効果の種類の、設定ファイルでの表記（"slow" など）
std::string_view toString(EffectType type);
/// 効果の種類の、画面に出す名前（"スロウ" など）
std::string_view displayName(EffectType type);
/// 設定ファイルの表記（"slow" / "snare" / "stun" / "knockup" / "polymorph" / "mark"）を効果の種類にする
/// @return 読めたかどうか（知らない表記なら false で、out は変えない）
bool effectTypeFromString(std::string_view text, EffectType& out);

/// JSON の配列から効果の一覧を作る。おかしな要素は警告ログを出して飛ばす
/// @param context 警告ログに出す場所の説明（どのキャラのどのスキルか）
std::vector<EffectSpec> parseEffects(const nlohmann::json& value, std::string_view context);

}  // namespace moba
