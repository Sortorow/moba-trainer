#pragma once
// スキルドッジモードの敵 AI
//
// 敵ごとに、一定間隔で次の 2 つを行う:
//   移動: スキルの射程に応じた距離を保ちながら自キャラを追う。賢いほど横移動が多く、他の敵と離れて囲むように動く。
//         自キャラがブッシュの中にいる時は、最も近い敵だけがそのブッシュの中に留まって、視界を保つ
//   スキル: 使えるスキルの中から、命中見込み（Aim.h）が最も高いものを選んで撃つ。自キャラが見えない間は撃たない
//   見失った時: まず、最後に見えた位置と、その時の動きから今いそうな位置を読んで追いかける（追跡。賢いほど先まで読む）。
//               着いても見つからなければ、敵全員で手分けして、まだ見ていないエリアを探す（探索。SearchMap.h）。
//               全エリアを探しても見つからなければ、最初から探し直す。誰か 1 体でも見つけたら、全員が元の動きに戻る。
//               賢さ 4 以上（probe）は、ブッシュの中や壁の向こうへ方向指定のスキルを撃って確かめ、当たればその位置へ向かう
// 賢さごとの数値は data/ai.jsonc で調整できる。

#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <vector>

#include "core/GameTypes.h"
#include "data/AiConfig.h"
#include "game/ai/Aim.h"
#include "game/ai/PlayerObserver.h"
#include "game/ai/SearchMap.h"

namespace moba {
class World;
struct Unit;
}  // namespace moba

namespace moba::ai {

/// スキルドッジの敵 AI。すべての敵の移動とスキルを決める
class EnemyDirector {
public:
    /// 敵全員の動き方
    enum class Mode {
        Fight,   // 自キャラが見えている（一度も見えていない時も含む）: 間合いを保って撃つ
        Chase,   // 見失った直後か、見えない自キャラにスキルが当たった: その位置へ追いかける
        Search,  // 追いかけた先にもいなかった: まだ見ていないエリアを手分けして探す
    };

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
    /// 今の動き方
    Mode mode() const { return mode_; }
    /// 探索のエリア分けと探索済みの記録（見失うまでは作られていない）
    const SearchMap& searchMap() const { return search_; }
    /// 見失ってから、全エリアを何周探し始めたか（見失った時に 1。見つけても戻さない）
    int searchRound() const { return searchRound_; }

private:
    /// 敵 1 体ごとの、次に撃てる時刻・移動先を決め直す時刻・横移動の向き
    struct Brain {
        EntityId unitId = kInvalidId;
        float nextCastTime = 0.0f;  // 次にスキルを撃ってよい時刻
        // 最後にスキルを撃った時刻（punishCC の最低限の間隔に使う。まだ撃っていなければ、十分に昔）
        float lastCastTime = -std::numeric_limits<float>::infinity();
        float replanTimer = 0.0f;  // 次に移動先を決め直すまでの時間
        float strafeTimer = 0.0f;  // 横移動の向きを変えるまでの時間
        float strafeSign = 1.0f;   // 横移動の向き（+1 / -1）

        // 探索
        int searchArea = -1;        // 探しに向かっているエリア（-1 なら、まだ決めていない）
        Vector2 progressPos{};      // 探索が進んでいるかを見るための、前に確かめた時の位置
        int progressUnseen = 0;     // 前に確かめた時の、向かっているエリアのまだ見ていない点の数
        float progressTime = 0.0f;  // 最後に探索が進んだ（動いた・点を見た）時刻
    };

    /// 敵の移動先を決める（スキルの射程に応じた距離を保ち、賢さに応じて横移動・他の敵と離れる動きを加える）。
    /// 自キャラがブッシュの中にいて、この敵が自キャラに最も近ければ、移動先をそのブッシュの中にする
    /// @param seenPlayer AI が認識している自キャラの位置（反応の遅れの分だけ前の位置）
    void planMovement(World& world, Brain& brain, Unit& enemy, Vector2 seenPlayer);
    /// brain の敵が、生きている敵の中で seenPlayer に最も近いか
    bool nearestToPlayer(World& world, const Brain& brain, Vector2 seenPlayer) const;
    /// 撃てる時なら、命中見込みが最も高いスキルを選んで撃つ（狙いのぶれも加える）。自キャラが見えている時だけ呼ぶ
    void tryCast(World& world, Brain& brain, Unit& enemy, const Unit& player);
    /// 見失った・見つけた・当たった を見て、動き方（mode_）を切り替える。探索の記録も更新する
    void updateMode(World& world, const Unit& player, bool playerVisible, float dt);
    /// 自キャラを見失った時に呼ぶ。追跡を始め、探索の記録を消す
    void startChase(World& world);
    /// 追跡を終えて探索に移るか（どれかの敵が追いかける先に着いた・時間がかかりすぎた）
    bool chaseFinished(World& world, float now) const;
    /// 追いかける先へ向かう
    void planChase(World& world, Unit& enemy, float now);
    /// 追いかける先（スキルが当たった位置か、見失った自キャラが今いそうな位置）を、enemy が立てる位置にしたもの
    Vector2 chaseGoal(const World& world, const Unit& enemy, float now) const;
    /// 見失った自キャラが今いそうな位置（最後に見えた位置 + その時の速度 × 見失ってからの時間。時間は chasePredict まで）
    Vector2 chasePoint(float now) const;
    /// 向かうエリアを決めて（他の敵と手分けする）、その中のまだ見ていない点へ向かう。たどり着けなければあきらめる
    void planSearch(World& world, Brain& brain, Unit& enemy);
    /// 賢さが probe の時、撃てるなら、ブッシュの中や壁の向こうへ確かめ撃ちをする
    void tryProbe(World& world, Brain& brain, Unit& enemy);
    /// 他の敵と同じ方向から攻めないように、自キャラから見た敵の方向をずらす
    /// @param away 自キャラから敵へ向かう向き
    /// @return ずらした後の向き
    Vector2 spreadDirection(World& world, const Brain& brain, Vector2 seenPlayer, Vector2 away) const;
    /// 自キャラと保つ距離（使うスキルの最短の射程 × engageRangeRatio を、決められた範囲に収めたもの）。
    /// 上限は maxEngageRange と「視界の半径 − visionMargin」の小さい方
    float engageRange(const World& world, const Unit& enemy) const;
    /// 着弾時に自キャラがいそうな位置の重み（賢さの設定から作る）
    /// @param playerStuck 自キャラが動けないか（動けないなら、今の位置だけを狙う）
    AimWeights aimWeights(bool playerStuck) const;
    /// AI が認識している自キャラの位置（反応の遅れの分だけ前の、見えていた位置）。
    /// 一度も見えていない時（最初から視界の外にいる時）は、位置の記録が無いので今の位置
    Vector2 seenPlayerPosition(const Unit& player, float now) const;
    /// min 以上 max 未満の乱数（max が min 以下なら min）
    float uniform(float min, float max);

    AiConfig config_;
    AiLevel level_;
    std::mt19937 rng_;
    PlayerObserver observer_;
    std::vector<Brain> brains_;

    // 見失った時
    Mode mode_ = Mode::Fight;
    float chaseStartTime_ = 0.0f;  // 追跡を始めた時刻
    std::optional<Vector2> clue_;  // 見えない自キャラにスキルが当たった位置（追跡はここへ向かう）
    Vector2 lostAt_{};             // 探す順番の基準（最後に見えた位置。スキルが当たったら、当たった位置）
    SearchMap search_;
    float markSeenTimer_ = 0.0f;  // 次に探索の記録（見た点）を更新するまでの時間
    int searchRound_ = 0;
};

}  // namespace moba::ai
