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
    void setup(const AiConfig& config, int aiLevel, std::uint32_t seed);
    void addEnemy(EntityId unitId);
    void update(World& world, float dt);

    const AiLevel& level() const { return level_; }
    std::size_t enemyCount() const { return brains_.size(); }

private:
    struct Brain {
        EntityId unitId = kInvalidId;
        float nextCastTime = 0.0f;  // 次にスキルを撃ってよい時刻
        float replanTimer = 0.0f;   // 次に移動先を決め直すまでの時間
        float strafeTimer = 0.0f;   // 横移動の向きを変えるまでの時間
        float strafeSign = 1.0f;    // 横移動の向き（+1 / -1）
    };

    void planMovement(World& world, Brain& brain, Unit& enemy, Vector2 seenPlayer);
    // playerVisible: 自キャラが見えているか（ブッシュに隠れている間は、最後に見えた位置を狙う）
    void tryCast(World& world, Brain& brain, Unit& enemy, const Unit& player, bool playerVisible);
    Vector2 spreadDirection(World& world, const Brain& brain, Vector2 seenPlayer, Vector2 away) const;
    float engageRange(const World& world, const Unit& enemy) const;
    AimWeights aimWeights(bool playerStuck) const;
    float uniform(float min, float max);

    AiConfig config_;
    AiLevel level_;
    std::mt19937 rng_;
    PlayerObserver observer_;
    std::vector<Brain> brains_;
};

}  // namespace moba::ai
