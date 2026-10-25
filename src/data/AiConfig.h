#pragma once
// スキルドッジモードの敵 AI の設定（data/ai.jsonc）

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace moba {

// 賢さ 1 段階分の設定
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

struct AiConfig {
    float enemyCooldownScale = 0.5f;   // 敵のスキルのクールダウン倍率
    float firstCastDelay = 1.5f;       // 開始から最初にスキルを撃つまでの時間
    float maxEngageRange = 1000.0f;    // 自キャラと取る距離の上限
    float playerReactionTime = 0.25f;  // AI が想定する、プレイヤーが弾を見て避け始めるまでの時間
    std::vector<AiLevel> levels;       // 賢さ 1〜5

    /// 賢さ（1 始まり）の設定。範囲外なら端の値を使う。levels が空なら既定値の設定
    const AiLevel& level(int aiLevel) const;
};

struct AiConfigLoadResult {
    AiConfig config;
    std::string error;  // 空なら成功
};

/// data/ai.jsonc を読み込む。値は決められた範囲に収める
/// @return 読み込んだ設定。読めない・"levels" が空の時は error に理由が入る
AiConfigLoadResult loadAiConfig(const std::filesystem::path& file);

}  // namespace moba
