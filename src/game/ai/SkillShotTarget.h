#pragma once
// スキルショットモードの的の AI
//
// 的は 1 体で、スキルを撃たない。毎刻み、次のどちらかを行う:
//   回避: 自キャラの弾・範囲に気づくと（反応の遅れの後）、確率で、当たらない位置へ避ける。
//         賢い的は、自キャラの詠唱（構え）を見た時点で、弾・範囲が出る前に避け始める
//   移動: 避けていない間は、自キャラとの距離を保ちながら、自キャラの周りのランダムな位置へ動く（ときどき止まる）
// 賢さごとの数値は data/ai.jsonc の "skillShot" で調整できる。

#include <cstdint>
#include <optional>
#include <random>
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

/// スキルショットの的の AI。的の移動と、自キャラのスキルの回避を決める
class SkillShotTarget {
public:
    /// 的の AI を準備する（賢さの設定を選び、乱数の種を決める）。的を決める前に呼ぶ
    /// @param aiLevel 的の賢さ（1〜5）
    /// @param seed 乱数の種（同じ種なら、同じ入力に対して同じ動きになる）
    void setup(const SkillShotConfig& config, int aiLevel, std::uint32_t seed);
    /// AI で動かす的を決める
    void setTarget(EntityId unitId) { unitId_ = unitId; }
    /// 的を最初に置く位置。自キャラから「保つ距離」の真ん中の、ランダムな方向（上下には maxVerticalOffset まで）
    Vector2 startPosition(Vector2 playerPos);
    /// 的を動かす（気づいた弾・範囲を避け、避けていない間は動き回る）。固定の時間刻みで呼ぶ
    void update(World& world, float dt);

    /// 的の通し番号（的がいなければ kInvalidId）
    EntityId unitId() const { return unitId_; }
    /// 使っている賢さの設定
    const SkillShotLevel& level() const { return level_; }
    /// 使っている的の設定（大きさ・移動速度・距離）
    const SkillShotConfig& config() const { return config_; }
    /// 今、弾・範囲を避けている途中か
    bool dodging() const { return dodging_; }
    /// 避け始めた回数（デバッグ表示用）
    int dodgeCount() const { return dodgeCount_; }

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
    /// threat を避けられる移動先を選ぶ（他の弾・範囲にも当たらず、障害物で止まらない位置）。見つからなければ nullopt
    std::optional<Vector2> chooseDodge(const World& world, const Unit& self, const std::vector<Threat>& threats,
                                       const Threat& threat);
    /// 自キャラとの距離を保ちながら、自キャラの周りのランダムな位置へ移動先を決め直す（ときどき止まる）。
    /// 上下には maxVerticalOffset より離れない
    void wander(const World& world, Unit& self, const Unit& player);
    /// min 以上 max 未満の乱数（max が min 以下なら min）
    float uniform(float min, float max);

    SkillShotConfig config_;
    SkillShotLevel level_;
    std::mt19937 rng_;
    EntityId unitId_ = kInvalidId;
    float moveTimer_ = 0.0f;  // 次に移動先を決め直すまでの時間
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
