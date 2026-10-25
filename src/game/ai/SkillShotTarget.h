#pragma once
// スキルショットモードの的の AI
//
// 的は 1 体で、スキルを撃たない。毎刻み、次のどちらかを行う:
//   回避: 自キャラの弾・範囲に気づくと（反応の遅れの後）、確率で、当たらない位置へ避ける。
//         賢い的は、自キャラの詠唱（構え）を見た時点で、弾・範囲が出る前に避け始める
//   移動: 避けていない間は、動き方のパターンを 1 つずつ行う。1 つ終わるたびに、重みで次のパターンを選ぶ
//         往復     自キャラから見て左右に、同じ幅で切り返す（賢いほど、回数・幅がばらつき、ときどきリズムを崩す）
//         接近     自キャラへ近づき、撃たれそうになったら（射程に入った・自キャラが止まった・構えた）急に左右へ切り返す
//         ランダム 自キャラの周りのランダムな位置へ動く
//         停止     立ち止まる
// 反応の遅れ（避け始め・接近からの切り返し）は、どの賢さでも 0.15 秒より短くならない（AiConfig.h）。
// 自キャラからは maxDistance より離れず、minDistance より近づかない。
// 賢さごとの数値は data/ai.jsonc の "skillShot" で調整できる。

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/GameTypes.h"
#include "data/AiConfig.h"
#include "game/ai/Aim.h"
#include "raylib.h"

namespace moba {
class World;
struct Unit;
}  // namespace moba

namespace moba::ai {

/// 的の動き方のパターン
enum class TargetPattern {
    Strafe,    // 往復（左右に切り返す）
    Approach,  // 接近して、撃たれそうになったら切り返す
    Wander,    // ランダムな位置へ動く
    Stop,      // 立ち止まる
};

/// パターンの、画面とログに出す名前（「往復」「接近」「ランダム」「停止」）
std::string_view displayName(TargetPattern pattern);

/// スキルショットの的の AI。的の移動（パターンの組み合わせ）と、自キャラのスキルの回避を決める
class SkillShotTarget {
public:
    /// 的の AI を準備する（賢さの設定を選び、乱数の種を決める）。的を決める前に呼ぶ
    /// @param aiLevel 的の賢さ（1〜5）
    /// @param seed 乱数の種（同じ種なら、同じ入力に対して同じ動きになる）
    void setup(const SkillShotConfig& config, int aiLevel, std::uint32_t seed);
    /// AI で動かす的を決める
    void setTarget(EntityId unitId) { unitId_ = unitId; }
    /// 的を最初に置く位置。自キャラから minDistance と maxDistance の真ん中の距離の、ランダムな方向
    Vector2 startPosition(Vector2 playerPos);
    /// 的を動かす（気づいた弾・範囲を避け、避けていない間は動き方のパターンを行う）。固定の時間刻みで呼ぶ
    void update(World& world, float dt);

    /// 的の通し番号（的がいなければ kInvalidId）
    EntityId unitId() const { return unitId_; }
    /// 使っている賢さの設定
    const SkillShotLevel& level() const { return level_; }
    /// 使っている的の設定（距離など）
    const SkillShotConfig& config() const { return config_; }
    /// 今、弾・範囲を避けている途中か
    bool dodging() const { return dodging_; }
    /// 避け始めた回数（デバッグ表示用）
    int dodgeCount() const { return dodgeCount_; }
    /// 今のパターンの種類（避けている途中は、避ける前のパターン）
    TargetPattern pattern() const { return pattern_.kind; }
    /// 今のパターンの説明（デバッグ表示用。「往復（残り 3 回・幅 200）」など）
    std::string patternLabel() const;

private:
    /// 的に当たりそうな、自キャラの弾・範囲（または詠唱中のスキル）1 つ
    struct Threat {
        int key = 0;  // 発動の番号（詠唱中のスキルは、詠唱ごとのマイナスの番号）
        AimShape shape = AimShape::Line;
        // Line: 弾の今の位置（詠唱中なら自キャラの位置）・向き・残りの飛距離・弾速・幅の半分
        Vector2 start{};
        Vector2 dir{1.0f, 0.0f};
        float length = 0.0f;
        float speed = 1.0f;
        float halfWidth = 0.0f;
        // Circle: 範囲の中心・半径
        Vector2 center{};
        float radius = 0.0f;
        float wait = 0.0f;  // Line: 弾が動き出すまでの時間（詠唱の残り）。Circle: 発動までの時間
    };

    /// 今行っているパターンの状態
    struct PatternState {
        TargetPattern kind = TargetPattern::Stop;
        float elapsed = 0.0f;  // パターンを始めてからの時間
        // 往復
        int swingsLeft = 0;  // 残りの切り返しの回数
        int swingIndex = 0;  // 何回目の切り返しか（0 始まり）
        float width = 0.0f;  // 往復の幅
        float side = 1.0f;   // 次に動く向き（自キャラから見て +1 = 右 / -1 = 左）
        // 接近
        float stopDistance = 0.0f;     // ここまで近づいたら切り返す
        float prevDistance = 0.0f;     // 前の刻みの、自キャラとの距離（射程に入った瞬間を見るため）
        bool playerWasMoving = false;  // 接近を始めてから、自キャラが動いていたことがあるか
        float jukeDelay = -1.0f;       // 切り返すまでの残り時間（反応の遅れ）。マイナスなら、まだ切り返しを決めていない
        bool juking = false;           // 切り返している途中か
        // 停止
        float stopTime = 0.0f;
    };

    /// 気づいていて、避けると決めた弾・範囲・詠唱の一覧（反応の遅れの前のもの・避けないと決めたものは含めない）
    std::vector<Threat> collectThreats(World& world, const Unit& self);
    /// 気づいた弾・範囲を避けるかどうか（1 つにつき 1 回だけ、dodgeChance の確率で決める）
    /// @param key 弾・範囲は発動の番号、詠唱中のスキルは詠唱ごとのマイナスの番号
    bool decide(int key);
    /// self が今の移動を続けると、threat に当たるか
    /// @param destination 今の移動先（止まっているなら self の位置）
    /// @param margin 当たり判定に足す余裕
    bool endangers(const Threat& threat, Vector2 pos, Vector2 destination, float speed, float radius,
                   float margin) const;
    /// threat を避けられる移動先を選ぶ（他の弾・範囲にも当たらず、障害物で止まらず、自キャラから maxDistance 以内の位置）。
    /// 見つからなければ nullopt
    std::optional<Vector2> chooseDodge(const World& world, const Unit& self, const std::vector<Threat>& threats,
                                       const Threat& threat);

    /// patterns の重みで次のパターンを選んで始める（自キャラから離れすぎていれば、接近にする）
    void startPattern(const World& world, Unit& self, const Unit& player);
    /// 今のパターンを 1 刻み進める。終わったら次のパターンを始める
    void updatePattern(const World& world, Unit& self, const Unit& player, float dt);
    /// 往復: 次の切り返しの移動先を決める（2 回切り返した後は、ときどきリズムを崩す）
    void nextSwing(const World& world, Unit& self, const Unit& player);
    /// 接近: 自キャラへ近づく。撃たれそうになったら、反応の遅れの後に切り返す
    void updateApproach(const World& world, Unit& self, const Unit& player, float dt);
    /// ランダム: 自キャラの周りのランダムな位置へ移動先を決める（障害物・ブッシュの中は選ばない）
    void wander(const World& world, Unit& self, const Unit& player);
    /// self から、自キャラに対して横（side = +1 なら右）へ distance だけ動く移動先。壁などで動けなければ反対側
    /// @param side 動いた向きを書き戻す（反対側へ動くことにした時は符号が変わる）
    Vector2 sideStep(const World& world, const Unit& self, const Unit& player, float& side, float distance) const;
    /// 移動先を、自キャラとの距離（minDistance〜maxDistance）・ステージの範囲・立てる位置に収める
    Vector2 keepInRange(const World& world, const Unit& self, Vector2 playerPos, Vector2 destination) const;
    /// 自キャラのヒット率に数えるスキルの、最も長い射程（maxDistance まで。接近で「射程に入った」の判定に使う）
    float playerSkillRange(const World& world, const Unit& player) const;
    /// min 以上 max 未満の乱数（max が min 以下なら min）
    float uniform(float min, float max);

    SkillShotConfig config_;
    SkillShotLevel level_;
    std::mt19937 rng_;
    EntityId unitId_ = kInvalidId;
    PatternState pattern_;
    bool patternActive_ = false;
    float playerStillTime_ = 0.0f;  // 自キャラが止まっている（移動も詠唱もしていない）時間
    bool dodging_ = false;
    int dodgeKey_ = 0;  // 今避けている弾・範囲・詠唱の番号
    int dodgeCount_ = 0;
    std::unordered_map<int, bool> decisions_;  // 気づいた弾・範囲・詠唱ごとの、避けるかどうか

    // 詠唱を見て避ける（readCast）ための記録
    bool playerWasCasting_ = false;
    int castSerial_ = 0;             // 自キャラの詠唱の通し番号（詠唱を見た時の判断の番号に使う）
    std::optional<bool> inherited_;  // 終わったばかりの詠唱で決めた判断（その詠唱から出た弾・範囲に引き継ぐ）
    float inheritedUntil_ = 0.0f;    // 引き継げる時刻の期限
};

}  // namespace moba::ai
