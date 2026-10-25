#pragma once
// スキルドッジモードの敵 AI
//
// 敵ごとに、一定間隔で次の 2 つを行う:
//   移動: スキルの射程に応じた距離を保ちながら自キャラを追う。賢いほど横移動が多く、他の敵と離れて囲むように動く
//   スキル: 使えるスキルの中から、命中見込み（Aim.h）が最も高いものを選んで撃つ
// 賢さごとの数値は data/ai.jsonc で調整できる。

#include <cstdint>
#include <random>
#include <vector>

#include "core/GameTypes.h"
#include "data/AiConfig.h"
#include "game/ai/Aim.h"
#include "game/ai/PlayerObserver.h"

namespace moba {
class World;
struct Unit;
}  // namespace moba

namespace moba::ai {

class EnemyDirector {
public:
    /// 敵 AI を準備する（賢さの設定を選び、乱数の種を決める）。敵を加える前に呼ぶ
    /// @param aiLevel 敵の賢さ（1〜5）
    /// @param seed 乱数の種（同じ種なら、同じ入力に対して同じ動きになる）
    void setup(const AiConfig& config, int aiLevel, std::uint32_t seed);
    /// 敵を 1 体加える（最初にスキルを撃つ時刻と、横移動の向きを決める）
    void addEnemy(EntityId unitId);
    /// すべての敵を動かし、撃てるならスキルを撃たせる。固定の時間刻みで呼ぶ
    void update(World& world, float dt);

    /// 使っている賢さの設定
    const AiLevel& level() const { return level_; }
    /// 敵の数
    std::size_t enemyCount() const { return brains_.size(); }

private:
    struct Brain {
        EntityId unitId = kInvalidId;
        float nextCastTime = 0.0f;  // 次にスキルを撃ってよい時刻
        float replanTimer = 0.0f;   // 次に移動先を決め直すまでの時間
        float strafeTimer = 0.0f;   // 横移動の向きを変えるまでの時間
        float strafeSign = 1.0f;    // 横移動の向き（+1 / -1）
    };

    /// 敵の移動先を決める（スキルの射程に応じた距離を保ち、賢さに応じて横移動・他の敵と離れる動きを加える）
    /// @param seenPlayer AI が認識している自キャラの位置（反応の遅れの分だけ前の位置）
    void planMovement(World& world, Brain& brain, Unit& enemy, Vector2 seenPlayer);
    /// 撃てる時なら、命中見込みが最も高いスキルを選んで撃つ（狙いのぶれも加える）
    /// @param playerVisible 自キャラが見えているか（ブッシュに隠れている間は、最後に見えた位置を狙う）
    void tryCast(World& world, Brain& brain, Unit& enemy, const Unit& player, bool playerVisible);
    /// 他の敵と同じ方向から攻めないように、自キャラから見た敵の方向をずらす
    /// @param away 自キャラから敵へ向かう向き
    /// @return ずらした後の向き
    Vector2 spreadDirection(World& world, const Brain& brain, Vector2 seenPlayer, Vector2 away) const;
    /// 自キャラと保つ距離（使うスキルの最短の射程 × engageRangeRatio を、決められた範囲に収めたもの）
    float engageRange(const World& world, const Unit& enemy) const;
    /// 着弾時に自キャラがいそうな位置の重み（賢さの設定から作る）
    /// @param playerStuck 自キャラが動けないか（動けないなら、今の位置だけを狙う）
    AimWeights aimWeights(bool playerStuck) const;
    /// min 以上 max 未満の乱数（max が min 以下なら min）
    float uniform(float min, float max);

    AiConfig config_;
    AiLevel level_;
    std::mt19937 rng_;
    PlayerObserver observer_;
    std::vector<Brain> brains_;
};

}  // namespace moba::ai
