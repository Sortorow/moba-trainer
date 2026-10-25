#pragma once
// 1 回のプレイ（モードの開始から終了まで）の設定。メニューで選び、モードの開始時に決まる

#include <cstdint>
#include <string>
#include <string_view>

namespace moba {

/// モードの種類
enum class GameMode {
    Sandbox,     // 練習場（ダミー相手にスキルを試す。制限時間なし）
    SkillDodge,  // スキルドッジ（敵のスキルを避ける）
    SkillShot,   // スキルショット（動き回る的にスキルを当てる）
};

/// モードの、画面とログに出す名前（「練習場」/「スキルドッジ」/「スキルショット」）
constexpr std::string_view displayName(GameMode mode) {
    switch (mode) {
        case GameMode::Sandbox: return "練習場";
        case GameMode::SkillDodge: return "スキルドッジ";
        case GameMode::SkillShot: return "スキルショット";
    }
    return "?";
}

/// 自キャラのクールダウンの扱い
enum class CooldownMode {
    Standard,  // 標準: キャラ設定のクールダウン × cooldownScale
    None,      // なし: スキルをすぐに再使用できる
};

/// 1 回のプレイの設定（メニューで選び、モードの開始時に決まる）
struct MatchConfig {
    GameMode mode = GameMode::Sandbox;
    std::string characterId;
    std::string stageId;  // data/stages の id。空や見つからない時は、一覧の最初のステージ
    CooldownMode cooldownMode = CooldownMode::Standard;
    float cooldownScale = 0.25f;  // 標準モードの倍率（1 = 実戦どおり、0.25 = 4 分の 1）

    // スキルドッジの設定
    int enemyCount = 3;  // 敵の数（1〜5）
    int aiLevel = 3;     // 敵の賢さ（1〜5）

    // スキルショットの設定
    int targetLevel = 3;  // 的の賢さ（1〜5）

    // 制限時間があるモード（スキルドッジ・スキルショット）の設定
    float timeLimitSeconds = 120.0f;  // 制限時間
    std::uint32_t seed = 0;           // 乱数の種（0 なら開始時に決める。ログに残るので、同じ展開を再現できる）

    /// 自キャラのクールダウンに実際に掛ける倍率（クールダウン「なし」なら 0）
    float playerCooldownScale() const { return cooldownMode == CooldownMode::None ? 0.0f : cooldownScale; }
    /// 制限時間があるモードか（スキルドッジ・スキルショット）
    bool timed() const { return mode != GameMode::Sandbox && timeLimitSeconds > 0.0f; }
};

}  // namespace moba
