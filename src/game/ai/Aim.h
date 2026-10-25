#pragma once
// 敵 AI の狙い（どこへ撃てば当たりやすいか）の計算。
// World に依存しない計算だけなので、テストで直接確かめられる。
//
// 考え方:
//   着弾した時に自キャラがいそうな位置を、候補として 4 つ挙げる
//   （その場に留まる / 今の移動を続ける / 弾を見て右へ避ける / 左へ避ける）。
//   賢さに応じて候補に重みを付け、候補の位置とその中間を狙い先として試し、命中見込みが最も高い所を選ぶ。

#include <optional>

#include "raylib.h"

namespace moba {
struct SkillDef;
}

namespace moba::ai {

enum class AimShape {
    Line,    // 直線に飛ぶ弾（方向指定）
    Circle,  // 地点に遅れて発生する円（地点指定）
};

// スキルの当たり方
struct AimModel {
    AimShape shape = AimShape::Line;
    float range = 0.0f;
    float windup = 0.0f;     // 詠唱時間
    float speed = 0.0f;      // Line: 弾速
    float halfWidth = 0.0f;  // Line: 弾の幅の半分
    float radius = 0.0f;     // Circle: 範囲の半径
    float delay = 0.0f;      // Circle: 詠唱を含めた、発動までの時間
};

/// スキル定義から当たり方を作る（方向指定 → Line、地点指定 → Circle）。狙いの計算に使えないスキルなら nullopt
std::optional<AimModel> aimModelFor(const SkillDef& def);

struct AimTarget {
    Vector2 pos{};        // AI が認識している位置
    Vector2 velocity{};   // AI が認識している速度
    float moveSpeed = 0;  // 最大移動速度
    float radius = 0;     // 当たり判定の半径
};

// 着弾時に自キャラがいそうな位置の重み
struct AimWeights {
    float stay = 1.0f;        // その場に留まる
    float keepMoving = 0.0f;  // 今の移動を続ける
    float dodgeRight = 0.0f;  // 弾の進行方向に対して右へ避ける
    float dodgeLeft = 0.0f;   // 左へ避ける
};

struct AimResult {
    Vector2 aimPoint{};         // カーソルを置く位置
    float expectedHit = 0.0f;   // 命中見込み（0〜1）
    float timeToImpact = 0.0f;  // 着弾までの見込み時間
};

/// 命中見込みが最も高くなる狙い先を選ぶ。着弾時に相手がいそうな位置の候補（留まる・移動を続ける・左右に避ける）に
/// weights の重みを付け、候補の位置とその中間を狙い先として試す
/// @param shooter 撃つ敵の位置
/// @param playerReactionTime 相手が弾を見て避け始めるまでの時間（これより早く着弾する弾は避けられない想定）
/// @return 最もよい狙い先と命中見込み。相手が射程外なら nullopt
std::optional<AimResult> chooseAim(Vector2 shooter, const AimModel& model, const AimTarget& target,
                                   const AimWeights& weights, float playerReactionTime);

/// aimPoint を狙って撃った時、位置 p にいる半径 targetRadius の相手に当たるか
bool wouldHit(Vector2 shooter, const AimModel& model, Vector2 aimPoint, Vector2 p, float targetRadius);

}  // namespace moba::ai
