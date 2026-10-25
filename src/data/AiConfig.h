#pragma once
// 敵 AI の設定（data/ai.jsonc）。スキルドッジの敵と、スキルショットの的

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace moba {

/// 賢さ 1 段階分の設定
struct AiLevel {
    std::string name;
    float reactionTime = 0.3f;     // 自キャラの動きを認識するまでの遅れ（秒）
    float castIntervalMin = 2.0f;  // スキルを撃つ間隔（この範囲でランダム）
    float castIntervalMax = 3.0f;
    float aimError = 60.0f;  // 狙いのぶれ（ゲーム内単位）

    // 狙う時に想定する「自キャラが着弾までにいそうな位置」の重み
    float predictStay = 1.0f;      // 今の位置に留まる
    float predictContinue = 0.0f;  // 今の移動を続ける（偏差撃ち）
    float predictDodge = 0.0f;     // 弾を見て横に避ける（避けた先を狙う）
    bool adaptive = false;         // 自キャラが左右どちらに避けやすいかを学習する

    float minHitChance = 0.0f;       // この命中見込みに満たない時は、しばらく撃つのを待つ
    float strafe = 0.0f;             // 横移動の多さ（0〜1）
    bool flank = false;              // 他の敵と離れて、囲むように動く
    bool punishCC = false;           // 自キャラが動けない時は、間隔を待たずにすぐ撃つ
    float engageRangeRatio = 0.85f;  // 自キャラとの距離を「スキルの射程 × この値」に保とうとする
};

/// スキルショットの的の、賢さ 1 段階分の設定
struct SkillShotLevel {
    std::string name;
    float reactionTime = 0.4f;     // 自キャラの弾・範囲が出てから、気づいて避け始めるまでの遅れ（秒）
    float dodgeChance = 0.0f;      // 気づいた弾・範囲を避けようとする確率（0〜1）
    bool readCast = false;         // 自キャラの詠唱を見て、弾・範囲が出る前に避け始める
    float moveIntervalMin = 1.5f;  // 移動先を決め直す間隔（この範囲でランダム）
    float moveIntervalMax = 3.0f;
    float stopChance = 0.3f;  // 移動先を決め直す時に、その場で止まる確率（0〜1）
    float sideRandom = 0.0f;  // 直線の弾を避ける時に、近い側ではなく遠い側へ避ける確率（0〜1。読まれにくくする）
};

/// スキルショットの的の設定（ai.jsonc の "skillShot"）。的の大きさ・移動速度・自キャラとの距離と、賢さ 1〜5 ごとの設定
struct SkillShotConfig {
    float targetRadius = 65.0f;      // 的の当たり判定の半径
    float targetMoveSpeed = 350.0f;  // 的の移動速度
    float distanceMin = 550.0f;      // 自キャラとの距離をこの範囲に保つ
    float distanceMax = 850.0f;
    float maxVerticalOffset = 400.0f;    // 自キャラから上下（画面の縦方向）に離れる距離の上限
    float dodgeMargin = 25.0f;           // 避ける時に、弾・範囲の端からさらに離れる距離
    std::vector<SkillShotLevel> levels;  // 賢さ 1〜5

    /// 賢さ（1 始まり）の設定。範囲外なら端の値を使う。levels が空なら既定値の設定
    const SkillShotLevel& level(int aiLevel) const;
};

/// 敵 AI の設定（data/ai.jsonc）。スキルドッジの敵（全体の設定と、賢さ 1〜5 ごとの設定）と、スキルショットの的
struct AiConfig {
    float enemyCooldownScale = 0.5f;   // 敵のスキルのクールダウン倍率
    float firstCastDelay = 1.5f;       // 開始から最初にスキルを撃つまでの時間
    float maxEngageRange = 1000.0f;    // 自キャラと取る距離の上限
    float playerReactionTime = 0.25f;  // AI が想定する、プレイヤーが弾を見て避け始めるまでの時間
    std::vector<AiLevel> levels;       // 賢さ 1〜5
    SkillShotConfig skillShot;         // スキルショットの的

    /// 賢さ（1 始まり）の設定。範囲外なら端の値を使う。levels が空なら既定値の設定
    const AiLevel& level(int aiLevel) const;
};

/// ai.jsonc の読み込み結果（設定か、読めなかった理由）
struct AiConfigLoadResult {
    AiConfig config;
    std::string error;  // 空なら成功
};

/// data/ai.jsonc を読み込む。値は決められた範囲に収める
/// @return 読み込んだ設定。読めない・"levels" や "skillShot" の "levels" が空の時は error に理由が入る
AiConfigLoadResult loadAiConfig(const std::filesystem::path& file);

}  // namespace moba
