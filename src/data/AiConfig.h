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
// 的の反応の遅れの下限（秒）。人が見てから反応できる速さの限界と言われる 0.15 秒より速くは反応させない
inline constexpr float kMinTargetReactionTime = 0.15f;

/// スキルショットの的の、賢さ 1 段階分の設定。避け方（反応・確率）と、動き方のパターンの混ぜ方の 2 つの軸がある
struct SkillShotLevel {
    std::string name;
    // 避け方
    float reactionTime = 0.5f;  // 自キャラの弾・範囲・詠唱に気づいて動き始めるまでの遅れ（秒。0.15 以上）
    float dodgeChance = 0.0f;   // 気づいた弾・範囲を避けようとする確率（0〜1）
    bool readCast = false;      // 自キャラの詠唱を見て、弾・範囲が出る前に避け始める
    float sideRandom = 0.0f;    // 直線の弾を避ける時に、近い側ではなく遠い側へ避ける確率（0〜1。読まれにくくする）

    // 動き方のパターン（次のパターンを選ぶ時の重み。賢いほど複数のパターンを混ぜる）
    float weightStrafe = 1.0f;     // 往復（左右に切り返す）
    float weightApproach = 0.0f;   // 接近して、撃たれそうになったら横へ切り返す
    float weightWander = 0.0f;     // ランダムな位置へ動く
    float weightStop = 0.0f;       // 立ち止まる
    float strafeSwingsMin = 4.0f;  // 往復で切り返す回数（この範囲でランダム。同じ値なら毎回同じ回数）
    float strafeSwingsMax = 4.0f;
    float strafeWidthMin = 200.0f;  // 往復の幅（1 回の往復の間は同じ幅。この範囲から往復ごとに選ぶ）
    float strafeWidthMax = 200.0f;
    float rhythmBreak = 0.0f;  // 往復のリズムを崩す確率（0〜1。2 回切り返した後から、1 回ごとに判定）
    float stopTimeMin = 0.8f;  // 立ち止まる時間（秒）
    float stopTimeMax = 1.5f;
};

/// スキルショットの的の大きさの選択肢 1 つ（メニューに出す名前と、当たり判定の半径）
struct TargetSize {
    std::string name;
    float radius = 65.0f;
};

/// スキルショットの的の設定（ai.jsonc の "skillShot"）。的の大きさ・移動速度の選択肢、自キャラとの距離と、賢さ 1〜5 ごとの設定
struct SkillShotConfig {
    std::vector<TargetSize> sizes;       // 的の大きさの選択肢（メニューで選ぶ）
    std::vector<float> moveSpeeds;       // 的の移動速度の選択肢（メニューで選ぶ）
    float minDistance = 300.0f;          // 自キャラにこれより近づかない
    float maxDistance = 1100.0f;         // 自キャラからこれより離れない
    float dodgeMargin = 25.0f;           // 避ける時に、弾・範囲の端からさらに離れる距離
    std::vector<SkillShotLevel> levels;  // 賢さ 1〜5

    /// 半径 radius の的の大きさの名前（選択肢に無ければ空）
    std::string sizeName(float radius) const;

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
